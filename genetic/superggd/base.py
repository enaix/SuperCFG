from enum import Enum, StrEnum


class ExecStatus(Enum):
    """Basic execution status for a program"""
    Compiling = 0,
    Running = 1,
    Exited = 2  # Indicates compilation error for a JIT compiler


class CompilationStrategy(StrEnum):
    """How to handle compilation errors"""
    Die = 0,
