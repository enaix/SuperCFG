from typing import Type, Union, Any


# TODO implement Term, NTerm and operators

class Term:
    def __init__(self, value: str):
        self.value = value

    def bake_supercfg(self, depth: int) -> str:
        return f"Term(cs<\"{self.value}\">())"

class TermsRange:
    def __init__(self, start: str, end: str):
        assert len(start) == 1 and len(end) == 1, "TermsRange start and end must be of length 1"
        self.start = start
        self.end = end

    def bake_supercfg(self, depth: int) -> str:
        return f"TermsRange(cs<\"{self.start}\">(), cs<\"{self.end}\">())"

class NTerm:
    def __init__(self, name: str):
        self.name = name

    def bake_supercfg(self, depth: int) -> str:
        return f"NTerm(cs<\"{self.name}\">())"


class BaseOp(type):
    """Base metaclass for the Operator type"""
    def __new__(cls, name, bases, attrs, num_ops: int):

        # add default bakery
        def bake_supercfg(instance, depth: int = 0) -> str:
            res = ""
            if name == "Grammar":
                res += "RulesDef"  # replace Grammar with RulesDef
            else:
                res += name  # otherwise we convert it literally

            # template parameters
            if hasattr(instance, "M"):
                res += "<" + str(instance.M)
                if hasattr(instance, "N"):
                    res += "," + str(instance.N)
                res += ">"

            res += "("
            for i in range(len(instance.ops)):
                res += instance.ops[i].bake_supercfg(depth + 1)
                if i < len(instance.ops) - 1:
                    res += ", "

            res += ")"

            if depth == 0:
                res += ";"
            return res

        def _repr(instance) -> str:
            return instance.bake_supercfg()  # temporary

        attrs["bake_supercfg"] = bake_supercfg
        attrs["__repr__"] = _repr

        def init(instance, ops: list[Union[Type[BaseOp], Term, TermsRange, NTerm]]):
            instance.ops = ops
            if num_ops != -1 and len(ops) != num_ops:
                raise TypeError(f"Operator {name} can only accept {num_ops} arguments")

        if "__init__" not in attrs:
            # add default init
            attrs["__init__"] = init
        else:
            # just run init with a decorator
            init_func = attrs["__init__"]

            def check_and_init(instance, *args, **kwargs):
                init_func(instance, *args, **kwargs)
                if num_ops != -1 and len(instance.ops) != num_ops:
                    raise TypeError(f"Operator {name} can only accept {num_ops} arguments")

            attrs["__init__"] = check_and_init

        return super().__new__(cls, name, bases, attrs)



class Concat(metaclass=BaseOp, num_ops=-1):
    pass

class Alter(metaclass=BaseOp, num_ops=-1):
    pass

class Define(metaclass=BaseOp, num_ops=2):
    pass # Two

class Optional(metaclass=BaseOp, num_ops=1):
    pass # One

class Repeat(metaclass=BaseOp, num_ops=1):
    pass # One

class Group(metaclass=BaseOp, num_ops=1):
    pass # One

class Comment(metaclass=BaseOp, num_ops=-1):
    pass

class SpecialSeq(metaclass=BaseOp, num_ops=-1):
    pass

class Except(metaclass=BaseOp, num_ops=1):
    pass # One

class End(metaclass=BaseOp, num_ops=-1):
    pass

class RepeatExact(metaclass=BaseOp, num_ops=1):
    def __init__(self, M: int, ops: list[Union[Type[BaseOp], Term, TermsRange, NTerm]]):
        self.M = M
        self.ops = ops

class RepeatGE(metaclass=BaseOp, num_ops=1):
    def __init__(self, M: int, ops: list[Union[Type[BaseOp], Term, TermsRange, NTerm]]):
        self.M = M
        self.ops = ops

class RepeatRange(metaclass=BaseOp, num_ops=1):
    def __init__(self, M: int, N: int, ops: list[Union[Type[BaseOp], Term, TermsRange, NTerm]]):
        self.M = M
        self.N = N
        self.ops = ops

class Grammar(metaclass=BaseOp, num_ops=-1):
    """Rules definition"""
    pass
