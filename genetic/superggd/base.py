from enum import Enum


class ExecStatus(Enum):
    """Basic execution status for a program"""
    Compiling = 0,
    Running = 1,
    Exited = 2
