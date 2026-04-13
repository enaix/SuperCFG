#!/usr/bin/env python3

import superggd
from superggd.base import *
from superggd.parsers import *
import argparse
import sys


# CLI entrypoint
# ==============

def main() -> None:
    ap = argparse.ArgumentParser(prog="superggd", description="genetic grammatical descent", formatter_class=argparse.ArgumentDefaultsHelpFormatter)

    sub = ap.add_subparsers(dest="command", required=False)
    gen_cmd = sub.add_parser("gen", help="Run the genetic algorithm")
    sub.add_parser("grid", help="Grid search (not yet implemented)")

    def _add_args(p: argparse.ArgumentParser) -> None:
        p.add_argument("--config", "-c", metavar="PATH", help="Path to a Python config file")
        p.add_argument("--module", "-m", metavar="PATH",
                       help="Path to the user module. Must expose SUPERGGD_MODULE_EXPORT object, which has the following functions: [grammar_generator(solution: np.ndarray, solution_idx: int) -> Grammar,  (optional) fitness_fn(solution, solution_idx, grammar, run_parser, pre_state) -> float,  pygad_params: dict|Callable[[],dict],  (optional) extra_genes: list[str]|Callable[[],list[str]]]")
        p.add_argument("--jobs", "-j", type=int, default=1, help="Number of parallel parser generator instances")
        p.add_argument("--comp-strategy", default="die", choices=["die", "skip"], help="Compilation error handling strategy (die: exit on error, skip: continue)")
        p.add_argument("--parser",  default="supercfg", choices=list(SUPERGGD_PARSER_GENERATORS.keys()), help="Parser backend")
        p.add_argument("--compilation-timeout", type=float, default=None, help="Max seconds to wait for batch compilation")

        s_cfg = p.add_argument_group(title="supercfg parser options")
        s_cfg.add_argument("--cling", "-l", metavar="PATH", default="cling", help="Path to the cling executable")
        s_cfg.add_argument("--supercfg","-s", metavar="PATH", default="../", help="Path to supercfg source code")
        s_cfg.add_argument("--cling-args", nargs="*", default=[], metavar="ARG", help="Extra command-line flags forwarded to cling, specified as a list of arguments")

    _add_args(ap)
    _add_args(gen_cmd)

    args, _ = ap.parse_known_args()

    if args.command == "grid":
        raise NotImplementedError("Grid search is not yet implemented")

    if args.module is None:
        ap.error("--module/-m is required in CLI mode")

    if args.config is not None:
        raise NotImplementedError("Config file management is not yet implemented")

    ggd = superggd.SuperGGD.from_module(args.module, num_parallel=args.jobs,
        compilation_strategy=CompilationStrategy(args.comp_strategy), compilation_timeout=args.compilation_timeout)

    ggd.init_parsers(SUPERGGD_PARSER_GENERATORS[args.parser], parser_args={"path_to_supercfg": args.supercfg, "path_to_cling": args.cling, "extra_cling_args": args.cling_args})

    ga = ggd.run()
    solution, fitness, idx = ga.best_solution()
    print(f"\nBest solution (index {idx}): {solution}")
    print(f"Best fitness : {fitness}")


if __name__ == "__main__":
    main()
