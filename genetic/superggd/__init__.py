#!/usr/bin/env python3

import concurrent.futures
import logging
import threading
from typing import Any, Callable, Optional

import numpy as np
import pygad

from superggd.parser import *
from superggd.base import *

logger = logging.getLogger(__name__)


# ======================================
# SuperGGD (Genetic Grammatical Descent)
# ======================================


class SuperGGD:
    """
    Main SuperGGD orchestrator. Wraps pygad.GA and drives batch grammar compilation so that:

    1. At the start of every generation all grammars are compiled in parallel.
    2. While compilation is in flight, an optional `pre_fitness_fn` runs for each grammar.
    3. Once all parsers are ready, `fitness_fn` is called with a bound `run_parser` callable.

    Parameters
    ----------
    grammar_generator:
        ``(solution: np.ndarray, solution_idx: int) -> Grammar``
        Maps a pygad gene vector to a Grammar object.

    fitness_fn:
        ``(solution, solution_idx, grammar, run_parser, pre_state) -> float``
        Called *after* compilation finishes.
        ``run_parser(input_string) -> Optional[ASTNode]``
        ``pre_state`` is whatever ``pre_fitness_fn`` returned (None if not set).

    pre_fitness_fn (optional):
        ``(solution, solution_idx, grammar) -> Any``
        Runs concurrently with parser compilation.  Its return value is forwarded as ``pre_state`` to ``fitness_fn``.

    num_parallel:
        How many ``SuperCFGParser`` instances to keep alive at once.

    path_to_cling / path_to_supercfg / extra_cling_args:
        Forwarded to every ``SuperCFGParser``.

    compilation_strategy:
        Forwarded to ``ParserManager``.

    compilation_timeout:
        Maximum seconds to wait for a full generation's batch to compile
        (passed to ``ParserManager.wait``).  None = wait forever.

    pygad_kwargs:
        Every remaining keyword argument is forwarded verbatim to ``pygad.GA``.  Do not pass ``fitness_func`` or ``on_generation``.
    """

    def __init__(
        self,
        grammar_generator: Callable[[np.ndarray, int], Any],
        fitness_fn: Callable,
        *,
        num_parallel: int = 1,
        path_to_cling: str = "cling",
        path_to_supercfg: str = "../",
        extra_cling_args: list[str] | None = None,
        compilation_strategy: Any = None,   # CompilationStrategy.Die
        compilation_timeout: Optional[float] = None,
        pre_fitness_fn: Optional[Callable] = None,
        **pygad_kwargs,
    ):

        if compilation_strategy is None:
            compilation_strategy = CompilationStrategy.Die

        self._grammar_generator = grammar_generator
        self._fitness_fn = fitness_fn
        self._pre_fitness_fn = pre_fitness_fn
        self._compilation_timeout = compilation_timeout

        # Parser infrastructure
        self._manager = ParserManager(num_parallel=num_parallel, compilation_strategy=compilation_strategy)
        self._parser_factory: Callable[[], Any] = lambda: SuperCFGParser(
            path_to_cling=path_to_cling,
            path_to_supercfg=path_to_supercfg,
            extra_cling_args=list(extra_cling_args or []),
        )

        # Per-generation state (reset at the top of every generation)
        self._gen_lock = threading.Lock()
        self._current_gen: int = -1                    # pygad generation counter
        self._current_grammars: dict[int, Any] = {}    # idx -> Grammar
        self._pre_states: dict[int, Any] = {}          # idx -> pre_fitness result
        self._compile_done = threading.Event()

        for reserved in ("fitness_func", "on_generation"):
            if reserved in pygad_kwargs:
                raise ValueError(f"SuperGGD manages '{reserved}' internally; do not pass it in pygad_kwargs")

        self._ga: pygad.GA = pygad.GA(fitness_func=self._fitness_wrapper, on_generation=self._on_generation, **pygad_kwargs)

    def run(self) -> pygad.GA:
        """Start the genetic algorithm.  Blocks until completion."""
        self._ga.run()
        return self._ga

    @property
    def ga(self) -> pygad.GA:
        """Direct access to the underlying pygad.GA instance."""
        return self._ga

    def best_solution(self):
        """Convenience wrapper around pygad.GA.best_solution()."""
        return self._ga.best_solution()


    def _start_generation(self, ga_instance: pygad.GA) -> None:
        """
        Called (once, under _gen_lock) at the start of each new generation.

        1. Generates Grammar objects for every individual in the population.
        2. Launches batch compilation via ParserManager.
        3. Runs pre_fitness_fn concurrently in a thread pool.
        4. Waits for compilation to finish before returning.
        """
        population: np.ndarray = ga_instance.population
        n = len(population)

        self._current_grammars = {}
        self._pre_states = {}
        self._compile_done.clear()

        # Generate grammars
        grammars: list[Any] = []
        for idx in range(n):
            try:
                g = self._grammar_generator(population[idx], idx)
            except Exception:
                logger.exception("grammar_generator raised for individual %d – skipping", idx)
                g = None
            self._current_grammars[idx] = g
            grammars.append(g)

        valid_indices = [i for i, g in enumerate(grammars) if g is not None]
        valid_grammars = [grammars[i] for i in valid_indices]

        # Create parsers & start batch compilation
        parsers = [self._parser_factory() for _ in valid_grammars]
        self._manager.compile_batch(valid_grammars, parsers)

        # Run pre_fitness concurrently with compilation
        if self._pre_fitness_fn is not None:
            def _run_pre(idx: int) -> tuple[int, Any]:
                grammar = self._current_grammars[idx]
                if grammar is None:
                    return idx, None
                try:
                    state = self._pre_fitness_fn(population[idx], idx, grammar)
                except Exception:
                    logger.exception("pre_fitness_fn raised for individual %d", idx)
                    state = None
                return idx, state

            with concurrent.futures.ThreadPoolExecutor(thread_name_prefix="superggd-pre") as pool:
                # Compilation is already running in the ParserManager's background event loop; pre-fitness runs here in parallel
                futures = {pool.submit(_run_pre, i): i for i in range(n)}
                for fut in concurrent.futures.as_completed(futures):
                    idx, state = fut.result()
                    self._pre_states[idx] = state

        # Wait for compilation to finish
        try:
            status_map = self._manager.wait(timeout=self._compilation_timeout)
        except concurrent.futures.TimeoutError:
            logger.error("Compilation timed out after %.1fs for generation %d", self._compilation_timeout, self._current_gen)
            status_map = self._manager.status()

        logger.debug("Generation %d: compilation status %s", self._current_gen, {str(g)[:40]: s for g, s in status_map.items()})

        self._compile_done.set()

    # pygad hooks
    # ===========

    def _fitness_wrapper(self, ga_instance: pygad.GA, solution: np.ndarray, solution_idx: int) -> float:
        """
        pygad fitness callback. Triggers batch compilation on the first call per generation, then delegates to the user's fitness_fn.

        pygad increments `generations_completed` after fitness evaluation, so during evaluation it still holds the previous generation's value. We use it as a stable per-generation identifier.
        """
        gen_id = ga_instance.generations_completed

        with self._gen_lock:
            if self._current_gen != gen_id:
                # First individual seen for this generation: kick everything off.
                self._current_gen = gen_id
                self._compile_done.clear()
                self._start_generation(ga_instance)
                # _start_generation blocks until compilation is done and sets _compile_done before returning.

        # Other threads may arrive here before _start_generation has finished; this wait is a cheap no-op once the event is already set.
        self._compile_done.wait()

        grammar = self._current_grammars.get(solution_idx)
        if grammar is None:
            logger.warning("No grammar for individual %d – returning 0.0", solution_idx)
            return 0.0

        def run_parser(input_string: str) -> Any:
            return self._manager.run(grammar, input_string)

        pre_state = self._pre_states.get(solution_idx)

        try:
            score = self._fitness_fn(solution, solution_idx, grammar, run_parser, pre_state)
        except Exception:
            logger.exception("fitness_fn raised for individual %d – returning 0.0", solution_idx)
            score = 0.0

        return float(score)

    def _on_generation(self, ga_instance: pygad.GA) -> None:
        """Called by pygad after each generation completes."""
        logger.info("Generation %d complete. Best fitness: %.4f", ga_instance.generations_completed, ga_instance.best_solution()[1])
        # Clean up parsers so they don't accumulate across generations.
        self._manager.reset()


# CLI entrypoint
# ==============

def main() -> None:
    """
    Command-line interface for SuperGGD.

    Base args:
        --config/-c    path to config file
        --cling/-l     path to cling executable
        --supercfg/-s  path to supercfg include folder
        --module/-m    path to the user module (CLI mode only)

    SuperGGD args:
        --parser              parser backend (default: supercfg)
        --comp-strategy       compilation error strategy (default: die)
        --jobs/-j             number of parallel parser instances
        --cling-args          extra arguments forwarded to cling
        --compilation-timeout max seconds to wait for batch compilation

    All remaining keyword arguments are passed straight through to pygad.GA
    via the user module's ``pygad_params`` dict.

    Commands:
        gen   – run the genetic algorithm (default)
        grid  – grid search (not yet implemented)

    User module interface
    ---------------------
    The module pointed to by --module must expose:
        grammar_generator(solution, idx)  -> Grammar
        fitness_fn(solution, idx, grammar, run_parser, pre_state) -> float
        pygad_params: dict                # forwarded to pygad.GA
    And optionally:
        pre_fitness_fn(solution, idx, grammar) -> Any
    """
    import argparse
    import importlib.util
    import sys

    ap = argparse.ArgumentParser(
        prog="superggd",
        description="genetic grammatical descent",
    )

    sub = ap.add_subparsers(dest="command", required=False)
    gen_cmd = sub.add_parser("gen",  help="Run the genetic algorithm")
    sub.add_parser("grid", help="Grid search (not yet implemented)")

    def _add_args(p: argparse.ArgumentParser) -> None:
        p.add_argument("--config",  "-c", metavar="PATH",
                       help="Path to a Python config file")
        p.add_argument("--cling",   "-l", metavar="PATH", default="cling",
                       help="Path to the cling executable (default: cling)")
        p.add_argument("--supercfg","-s", metavar="PATH", default="../",
                       help="Path to the supercfg include folder (default: ../)")
        p.add_argument("--module",  "-m", metavar="PATH",
                       help="Path to the user module")

        p.add_argument("--parser",  default="supercfg",
                       help="Parser backend (default: supercfg)")
        p.add_argument("--comp-strategy", default="die", choices=["die"],
                       help="Compilation error strategy (default: die)")
        p.add_argument("--jobs", "-j", type=int, default=1,
                       help="Number of parallel parser instances (default: 1)")
        p.add_argument("--cling-args", nargs="*", default=[], metavar="ARG",
                       help="Extra arguments forwarded to cling")
        p.add_argument("--compilation-timeout", type=float, default=None,
                       help="Max seconds to wait for batch compilation (default: none)")

    _add_args(ap)
    _add_args(gen_cmd)

    args, _ = ap.parse_known_args()

    if args.command == "grid":
        print("Grid search is not yet implemented.", file=sys.stderr)
        sys.exit(1)

    if args.module is None:
        ap.error("--module/-m is required in CLI mode")

    # load user module
    spec = importlib.util.spec_from_file_location("_superggd_user_module", args.module)
    if spec is None or spec.loader is None:
        print(f"Cannot load module: {args.module}", file=sys.stderr)
        sys.exit(1)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)  # type: ignore[union-attr]

    for attr in ("grammar_generator", "fitness_fn", "pygad_params"):
        if not hasattr(mod, attr):
            print(f"User module must define '{attr}'.", file=sys.stderr)
            sys.exit(1)

    ggd = SuperGGD(
        grammar_generator=mod.grammar_generator,
        fitness_fn=mod.fitness_fn,
        pre_fitness_fn=getattr(mod, "pre_fitness_fn", None),
        num_parallel=args.jobs,
        path_to_cling=args.cling,
        path_to_supercfg=args.supercfg,
        extra_cling_args=args.cling_args,
        compilation_strategy=CompilationStrategy(args.comp_strategy),
        compilation_timeout=args.compilation_timeout,
        **mod.pygad_params,
    )

    ga = ggd.run()
    solution, fitness, idx = ga.best_solution()
    print(f"\nBest solution (index {idx}): {solution}")
    print(f"Best fitness : {fitness}")


if __name__ == "__main__":
    main()
