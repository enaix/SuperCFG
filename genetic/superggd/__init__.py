import concurrent.futures
import logging
import threading
from typing import Any, Callable, Optional, Type
import sys
import importlib.util
import numpy as np
import pygad

from superggd.parser import *
from superggd.parsers import *
from superggd.base import *

logger = logging.getLogger(__name__)


# ======================================
# SuperGGD (Genetic Grammatical Descent)
# ======================================


class SuperGGD:
    """
    Main SuperGGD instance, handles ``pygad.GA`` and parser generators management. At each step executes parser generators and ``pre_fitness_fn`` in parallel. Also see ``init_parsers``.

    Parameters
    ----------
    grammar_generator:
        ``(solution: np.ndarray, solution_idx: int) -> Grammar``
        Maps a pygad gene vector to a Grammar object.

    fitness_fn:
        ``(solution, solution_idx, grammar, run_parser, pre_fn_result) -> float``
        Called *after* all parser generators are compiled and all ``pre_fitness_fn`` exited.
        ``run_parser(input_string) -> Optional[ASTNode]``
        ``pre_fn_result`` is the ``pre_fitness_fn`` result (None if not set).

    pre_fitness_fn (optional):
        ``(solution, solution_idx, grammar) -> Any``
        Runs concurrently with parser compilation. Its return value is forwarded as ``pre_fn_result`` to ``fitness_fn``.

    num_parallel:
        How many parser generators to compile at once. Ideally should match the number of CPU cores, limited by the amount of available memory.

    compilation_strategy:
        Specifies how to handle parser generator errors. Forwarded to ``superggd.parser.ParserManager``.

    compilation_timeout:
        How long to wait for parser generators to compile (in fractional seconds), passed to ``ParserManager.wait``. Set None to wait forever.

    extra_genes:
        Specifies which genes are not used in the grammar_generator.

    pygad_kwargs:
        Every remaining keyword argument is forwarded to ``pygad.GA``. Do not pass ``fitness_func`` or ``on_generation``.
    """

    def __init__(self, grammar_generator: Callable[[np.ndarray, int], Any],
                 fitness_fn: Callable, *,
                 pre_fitness_fn: Optional[Callable] = None,
                 num_parallel: int = 1,
                 compilation_strategy: Any = None,   # CompilationStrategy.Die
                 compilation_timeout: Optional[float] = None,
                 extra_genes: Optional[list[str]] = None,
                 **pygad_kwargs):

        if compilation_strategy is None:
            compilation_strategy = CompilationStrategy.Die
        self._grammar_generator = grammar_generator
        self._fitness_fn = fitness_fn
        self._pre_fitness_fn = pre_fitness_fn
        self._compilation_timeout = compilation_timeout
        if extra_genes is None:
            self._extra_genes: list[str] = []
        else:
            self._extra_genes = extra_genes
        self._module: Optional[Any] = None

        # Parser infrastructure
        self._manager = ParserManager(num_parallel=num_parallel, compilation_strategy=compilation_strategy)
        self._parser_class: Type[Any] = SuperCFGParser
        self._fallback_parser_class: Optional[Type[Any]] = None
        self._parser_args: dict[str, Any] = {}
        self._fallback_parser_args: dict[str, Any] = {}

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

    @staticmethod
    def from_module(module: Union[os.PathLike, Any], **kwargs) -> SuperGGD:
        """Initialize SuperGGD with the specified module, kwargs must not include ``grammar_generator``, ``fitness_fn``, ``pre_fitness_fn`` and ``extra_genes``. kwargs may also define module-specific args"""
        restricted_params = ["grammar_generator", "fitness_fn", "pre_fitness_fn", "extra_genes"]
        if any([x in kwargs for x in restricted_params]):
            raise ValueError("kwargs must not include grammar_generator, fitness_fn, pre_fitness_fn and extra_genes")

        if isinstance(module, os.PathLike):
            spec = importlib.util.spec_from_file_location("_superggd_user_module", module)
            if spec is None or spec.loader is None:
                print(f"Cannot load module: {module}", file=sys.stderr)
                sys.exit(1)
            mod_raw = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod_raw)
            if not hasattr(mod_raw, "SUPERGGD_MODULE_EXPORT"):
                raise ValueError(f"Module {module} must define SUPERGGD_MODULE_EXPORT with the main object")
            mod = mod_raw.SUPERGGD_MODULE_EXPORT
        else:
            mod = module  # do nothing, expect it to be mod.SUPERGGD_MODULE_EXPORT

        get_callable_or_obj = lambda x: x() if callable(x) else x  # If x is a function, call it, return it as-is otherwise

        # init module args
        if hasattr(mod, "init_args"):
            if not callable(mod.init_args):
                raise ValueError(f"In module {module}, SUPERGGD_MODULE_EXPORT.init_args must be a callable")
            mod.init_args(**kwargs)

        if hasattr(mod, "post_init"):
            if not callable(mod.post_init):
                raise ValueError(f"In module {module}, SUPERGGD_MODULE_EXPORT.post_init must be a callable")
            mod.post_init()

        for attr in ("grammar_generator", "fitness_fn", "pygad_params"):
            if not hasattr(mod, attr):
                raise ValueError(f"User module {module} must define SUPERGGD_MODULE_EXPORT.{attr}")

        # get pygad params
        params = get_callable_or_obj(mod.pygad_params)
        if not isinstance(params, dict):
            raise ValueError(f"In module {module}, SUPERGGD_MODULE_EXPORT.pygad_params must be (or return) a dict")
        if kwargs.get("pygad_params") is not None:
            kwargs["pygad_params"] = params | kwargs["pygad_params"]  # Overload elements in module with ones specified in kwargs
        else:
            kwargs["pygad_params"] = params

        # get extra genes
        if hasattr(mod, "extra_genes"):
            kwargs["extra_genes"] = get_callable_or_obj(mod.extra_genes)
            if not all([isinstance(x, str) for x in kwargs["extra_genes"]]):
                raise ValueError(f"In module {module}, SUPERGGD_MODULE_EXPORT.extra_genes must be (or return) list[str]")

        # get default superggd params
        if hasattr(mod, "defaults"):
            kw_defaults = get_callable_or_obj(mod.defaults)
            if not isinstance(kw_defaults, dict):
                raise ValueError(f"In module {module}, SUPERGGD_MODULE_EXPORT.defaults must be (or return) a dict")
            all_restricted = restricted_params + ["pygad_params", "extra_genes"]
            if any([x in kw_defaults for x in all_restricted]):
                raise ValueError(f"In module {module}, SUPERGGD_MODULE_EXPORT.defaults must not export {all_restricted}")

            kwargs = kw_defaults | kwargs  # Overload elements in defaults with user-provided kwargs

        ggd = SuperGGD(grammar_generator=mod.grammar_generator, fitness_fn=mod.fitness_fn, pre_fitness_fn=getattr(mod, "pre_fitness_fn"), **kwargs)

        # get default parsers params, will be overriden with the next init_parsers() call
        if hasattr(mod, "parsers_defaults"):
            parsers_params = get_callable_or_obj(mod.parsers_defaults)
            if not isinstance(parsers_params, dict):
                raise ValueError(f"In module {module}, SUPERGGD_MODULE_EXPORT.parsers_defaults must be (or return) a dict")
            ggd.init_parsers(**parsers_params)

        ggd._module = mod  # Make sure that the module is not unloaded by the gc
        return ggd

    def init_parsers(self, parser_class: Optional[Type[Any]] = None, fallback_parser_class: Optional[Type[Any]] = None, parser_args: Optional[dict[str, Any]] = None, fallback_parser_args: Optional[dict[str, Any]] = None) -> None:
        """
        Initialize parser generators as the ``parser_class`` and ``fallback_parser_class`` class types with parser_args/fallback_parser_args dictionary. ``superggd.parsers.SuperCFGParser`` will be used as the default parser generator.
        If the strategy is ``CompilationStrategy.Die``, fallback_parser_class is not needed.
        If init_parsers is called twice (example: module has defined parsers_defaults), the next call is going to overload the previous args. Args of type dict are merged.

        SuperCFG (default parser) parameters:

        path_to_cling:
            Path to cling binary, see ``superggd.parser.SuperCFGParser``

        path_to_supercfg:
            Path to SuperCFG source code, see ``superggd.parser.SuperCFGParser``

        extra_cling_args:
            Extra cling command-line arguments, see ``superggd.cling.ClingInstance``
        """
        def update_if_none(lhs, rhs):
            if rhs is not None:
                if isinstance(lhs, dict):
                    lhs = lhs | rhs  # Overload old args
                else:
                    lhs = rhs

        update_if_none(self._parser_class, parser_class)
        update_if_none(self._fallback_parser_class, fallback_parser_class)
        update_if_none(self._parser_args, parser_args)
        update_if_none(self._fallback_parser_args, fallback_parser_args)

    def run(self) -> pygad.GA:
        """Start the genetic algorithm. Blocks until completion."""
        self._ga.run()
        return self._ga

    @property
    def ga(self) -> pygad.GA:
        """Direct access to the underlying pygad.GA instance."""
        return self._ga

    def best_solution(self):
        """Convenience wrapper around pygad.GA.best_solution()."""
        return self._ga.best_solution()

    def _new_parser(self) -> Any:
        """Create new parser instance"""
        if self._parser_class is None or self._parser_args is None:
            raise ValueError("init_parsers() was not called. Configure parser generators using SuperGGD.init_parsers()")
        return self._parser_class(**self._parser_args)

    def _start_generation(self, ga_instance: pygad.GA) -> None:
        """
        Called at the start of each new generation. Handles parsers compilation and fitness_fn/pre_fitness_fn execution
        """
        population: np.ndarray = ga_instance.population
        n = len(population)

        # TODO properly log history
        self._current_grammars = {}
        self._pre_states = {}
        self._compile_done.clear()

        # Generate grammars
        grammars: list[Any] = []
        for idx in range(n):
            try:
                g = self._grammar_generator(population[idx], idx)
            except Exception as e:
                if self._manager.comp_strategy == CompilationStrategy.Die:
                    raise RuntimeError(f"grammar_generator raised for individual {idx}") from e
                else:
                    logger.exception(f"grammar_generator raised for individual {idx} : {e}")
                g = None
            self._current_grammars[idx] = g
            grammars.append(g)

        valid_indices = [i for i, g in enumerate(grammars) if g is not None]
        valid_grammars = [grammars[i] for i in valid_indices]

        parsers = [self._new_parser() for _ in valid_grammars]
        self._manager.compile_batch(valid_grammars, parsers)

        if self._pre_fitness_fn is not None:
            def _run_pre(idx: int) -> tuple[int, Any]:
                grammar = self._current_grammars[idx]
                if grammar is None:
                    return idx, None
                try:
                    if self._pre_fitness_fn is not None:
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
        pygad fitness callback, waits for the compilation to finish and then executes user's fitness function.
        """
        # this is used as the unique generation identifier
        gen_id = ga_instance.generations_completed

        with self._gen_lock:
            if self._current_gen != gen_id:
                # new generation
                self._current_gen = gen_id
                self._compile_done.clear()
                self._start_generation(ga_instance)  # blocks until the compilation is done

        self._compile_done.wait()

        grammar = self._current_grammars.get(solution_idx)
        if grammar is None:
            logger.warning("No grammar for individual %d – returning 0.0", solution_idx)
            return 0.0

        def run_parser(input_string: str) -> Any:
            return self._manager.run(grammar, input_string)

        pre_fn_result = self._pre_states.get(solution_idx)

        try:
            score = self._fitness_fn(solution, solution_idx, grammar, run_parser, pre_fn_result)
        except Exception as e:
            logger.exception(f"fitness_fn raised for individual {solution_idx} – returning 0.0 : {e}")
            score = 0.0

        return float(score)

    def _on_generation(self, ga_instance: pygad.GA) -> None:
        """Called by pygad after each generation completes."""
        logger.info("Generation %d complete. Best fitness: %.4f", ga_instance.generations_completed, ga_instance.best_solution()[1])
        self._manager.reset()
