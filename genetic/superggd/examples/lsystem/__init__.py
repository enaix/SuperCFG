from typing import Any, Optional


class LSystem:
    def __init__(self) -> None:
        self._mapping_type: Optional[str] = None
        self._target: Optional[str] = None

    def populate_argparse_group(self, group: Any) -> None:
        group.add_argument("--lsystem", required=True, help="target lsystem string")
        group.add_argument("--mapping-type", choices=["lex", "seed"], default="seed", help="lsystem string candidates -> gene mapping algorithm")

    def init_args(self, args: dict[str, Any]) -> None:
        self._mapping_type = args.get("mapping_type")
        self._target = args.get("lsystem")

    def post_init(self) -> None:
        if self._target is None:
            raise ValueError("No target to optimize, please specify \"lsystem\" argument")

    def grammar_generator(self, genome):
        pass



SUPERGGD_MODULE_EXPORT = LSystem()
