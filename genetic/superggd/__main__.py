#!/usr/bin/env python3

import superggd
from superggd.base import *
from superggd.parsers import *
import argparse
import sys
import importlib.util
from typing import Callable, Optional, Any


# CLI entrypoint
# ==============

def main() -> None:
    def _get_parsers(add_help: bool = True):
        ap = argparse.ArgumentParser(prog="superggd", description="Genetic grammatical descent", formatter_class=argparse.ArgumentDefaultsHelpFormatter, add_help=add_help)

        sub = ap.add_subparsers(dest="command", required=False)
        gen_cmd = sub.add_parser("gen", help="Run the genetic algorithm")
        sub.add_parser("grid", help="Grid search (not yet implemented)")
        return ap, sub, gen_cmd

    def _add_args(p: argparse.ArgumentParser) -> None:
        p.add_argument("--config", "-c", metavar="PATH", help="Path to a Python config file")
        p.add_argument("--module", "-m", metavar="PATH",
                       help="Path to the user module. Must expose SUPERGGD_MODULE_EXPORT object, which has the following functions: [grammar_generator(solution: np.ndarray, solution_idx: int) -> Grammar,  (optional) fitness_fn(solution, solution_idx, grammar, run_parser, pre_state) -> float,  pygad_params: dict|Callable[[],dict],  (optional) extra_genes: list[str]|Callable[[],list[str]]]")
        p.add_argument("--jobs", "-j", type=int, help="Number of parallel parser generator instances")
        p.add_argument("--comp-strategy", choices=["die", "skip"], help="Compilation error handling strategy (die: exit on error, skip: continue)")
        p.add_argument("--parser", choices=list(SUPERGGD_PARSER_GENERATORS.keys()), help="Parser backend")
        p.add_argument("--compilation-timeout", type=float, help="Max seconds to wait for batch compilation")

        s_cfg = p.add_argument_group(title="supercfg parser options")
        s_cfg.add_argument("--cling", "-l", metavar="PATH", default="cling", help="Path to the cling executable")
        s_cfg.add_argument("--supercfg","-s", metavar="PATH", default="../", help="Path to supercfg source code")
        s_cfg.add_argument("--cling-args", nargs="*", metavar="ARG", help="Extra command-line flags forwarded to cling, specified as a list of arguments")

    module_subgroup_title = "module-specific options"
    def _add_module_group(p: argparse.ArgumentParser, populate_argparse_group: Callable):
        subgroup = p.add_argument_group(title=module_subgroup_title)
        populate_argparse_group(subgroup)

    # First parser without module-specific args. Must not print help
    ap, sub, gen_cmd = _get_parsers(add_help=False)
    _add_args(ap)
    _add_args(gen_cmd)
    args, unknown_args = ap.parse_known_args()
    # Start populating the second parser, prints full help
    ap_full, sub_full, gen_cmd_full = _get_parsers()
    _add_args(ap_full)
    _add_args(gen_cmd_full)

    if args.command == "grid":
        raise NotImplementedError("Grid search is not yet implemented")

    if args.module is None:
        # Avoid calling ap.error to prevent usage prints
        if "--help" in unknown_args or "-h" in unknown_args:
            # Print help without module-specific args
            add_placeholder_arg = lambda group: group.add_argument("--foo", help="(Placeholder) Module-defined arguments will appear here")
            _add_module_group(ap_full, add_placeholder_arg)
            _add_module_group(gen_cmd_full, add_placeholder_arg)
            ap_full.parse_args()  # will print help and exit

        ap_full.error("--module/-m is required in CLI mode")

    if args.config is not None:
        raise NotImplementedError("Config file management is not yet implemented")

    # Load module
    spec = importlib.util.spec_from_file_location("_superggd_user_module", args.module)
    if spec is None or spec.loader is None:
        print(f"Cannot load module: {args.module}", file=sys.stderr)
        sys.exit(1)
    mod_raw = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod_raw)
    if not hasattr(mod_raw, "SUPERGGD_MODULE_EXPORT"):
        raise ValueError(f"Module {args.module} must define SUPERGGD_MODULE_EXPORT with the main object")
    mod = mod_raw.SUPERGGD_MODULE_EXPORT

    # Get module-specific args
    if hasattr(mod, "populate_argparse_group"):
        if not callable(mod.populate_argparse_group):
            raise ValueError(f"In module {args.module}, SUPERGGD_MODULE_EXPORT.populate_argparse_group must be a callable")
        _add_module_group(ap_full, mod.populate_argparse_group)  # must accept argparse.ArgumentGroup
        _add_module_group(gen_cmd_full, mod.populate_argparse_group)

    # Module loading finished, need to parse module args & print help
    args_full = ap_full.parse_args()
    module_args: Optional[dict[str, Any]] = None
    for group in ap_full._action_groups:
        if group.title == module_subgroup_title:
            module_args = {a.dest : getattr(args, a.dest, None) for a in group._group_actions}
            break

    assert module_args is not None, "superggd.__main__ : argparse has no module subgroup"

    # We need to make sure that only explicitly-specified arguments are provided, so that the module can supply its own defaults
    # Ideally, the parser should display module-defined defaults
    kwargs = {"num_parallel": getattr(args, "jobs", None), "compilation_timeout": getattr(args, "compilation_timeout", None)}
    if hasattr(args, "comp_strategy"):
        kwargs["compilation_strategy"] = CompilationStrategy(args.comp_strategy)

    # Load module args
    if kwargs.keys() & module_args.keys():
        raise ValueError(f"In module {args.module}: SUPERGGD_MODULE_EXPORT.populate_argparse_group defines conflicting args: {kwargs.keys() & module_args.keys()}")
    kwargs = kwargs | module_args  # Do not intersect

    ggd = superggd.SuperGGD.from_module(mod, **kwargs)

    # Same for init_parsers
    if hasattr(args, "parser"):
        parser_class = SUPERGGD_PARSER_GENERATORS[args.parser]
    else:
        parser_class = None
    ggd.init_parsers(parser_class, parser_args={"path_to_supercfg": args.supercfg, "path_to_cling": args.cling, "extra_cling_args": getattr(args, "cling_args", None)})

    ga = ggd.run()
    solution, fitness, idx = ga.best_solution()
    print(f"\nBest solution (index {idx}): {solution}")
    print(f"Best fitness : {fitness}")


if __name__ == "__main__":
    main()
