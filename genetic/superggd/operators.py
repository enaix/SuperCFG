from typing import Type


# TODO implement Term, NTerm and operators


class BaseOp:
    def __init__(self, *ops: list[Type[BaseOp]]):
        self.ops = ops

class Grammar(BaseOp):
    pass
