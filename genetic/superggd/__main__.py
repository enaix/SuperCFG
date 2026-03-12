#!/usr/bin/env python3

import superggd
from superggd.base import *
import argparse
import importlib.util
import sys


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
        grammar_generator(solution: np.ndarray, solution_idx: int)  -> Grammar
        fitness_fn(solution, solution_idx, grammar, run_parser, pre_state) -> float
        pygad_params: dict                # forwarded to pygad.GA
    And optionally:
        pre_fitness_fn(solution, solution_idx, grammar) -> Any
    """

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

    ggd = superggd.SuperGGD(
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
