from enum import Enum, StrEnum
from typing import Callable

from dataclasses import dataclass, field


class ExecStatus(Enum):
    """Basic execution status for a program"""
    Compiling = 0,
    Running = 1,
    Exited = 2  # Indicates compilation error for a JIT compiler


class CompilationStrategy(StrEnum):
    """How to handle compilation errors"""
    Die = "die",  # Give up on the grammar and stop SuperGGD
    Skip = "skip", # Give up on the grammar and continue execution


@dataclass
class ASTNode:
    """Abstract syntax tree node"""
    name: str
    value: str
    children: list[ASTNode] = field(default_factory=list)

    def pformat(self, _depth: int = 0) -> str:
        """Return a human-readable representation"""
        indent = "|  " * _depth
        header = f"{indent}{self.name!r} ({len(self.children)} children) : {self.value!r}"
        child_lines = [c.pformat(_depth + 1) for c in self.children]
        return "\n".join([header] + child_lines)

    def each(self, fn: Callable, depth: int = 0) -> None:
        """Apply fn recursively to every AST child, fn must accept the AST node, depth and is_leaf arguments"""
        fn(self, depth=depth, is_leaf=(len(self.children) == 0))
        for child in self.children:
            child.each(fn, depth + 1)

    def __repr__(self) -> str:
        return (f"ASTNode(name={self.name!r}, value={self.value!r}, children=[{len(self.children)} nodes])")


def encode_input_hex(s: str) -> str:
    """Encode ASCII input string to hex"""
    try:
        res = s.encode("ascii").hex()
    except UnicodeEncodeError as e:
        raise ValueError("encode_input_hex() only works with ASCII strings") from e
    return res


# Wire ast representation deserealizer
# Only handles ASCII
# ====================================


def _decode_field(data: str, pos: int) -> tuple[str, int]:
    """
    Parse one <decimal_len>:<hex_body> field starting at pos.

    Returns
    -------
    (decoded_string, new_pos)
    """
    colon = data.find(':', pos)
    if colon == -1:
        raise ValueError(f"Expected \':\' after byte-length at pos {pos}")

    try:
        byte_len = int(data[pos:colon])
    except ValueError:
        raise ValueError(f"Non-integer byte-length \'{data[pos:colon]}\' at pos {pos}")

    hex_start = colon + 1
    hex_end = hex_start + byte_len * 2  # 2 hex digits per byte

    if hex_end > len(data):
        raise ValueError(f"Hex body truncated: need {byte_len * 2} chars at pos {hex_start}, but string ends at {len(data)}")

    hex_body = data[hex_start:hex_end]
    try:
        decoded = bytes.fromhex(hex_body).decode("ascii")
    except (ValueError, UnicodeDecodeError) as exc:
        raise ValueError(f"Bad hex body at pos {hex_start}: {exc}") from exc

    return decoded, hex_end


def _decode_node(data: str, pos: int) -> tuple[ASTNode, int]:
    """
    Parse one node (and all its descendants) starting at pos.

    Returns
    -------
    (ASTNode, new_pos)
    """
    name, pos = _decode_field(data, pos)

    if pos >= len(data) or data[pos] != '|':
        raise ValueError(f"Expected \'|\' after name field at pos {pos}")
    pos += 1  # consume '|'

    value, pos = _decode_field(data, pos)

    if pos >= len(data) or data[pos] != '|':
        raise ValueError(f"Expected '|' after value field at pos {pos}")
    pos += 1  # consume '|'

    pipe = data.find('|', pos)
    if pipe == -1:
        raise ValueError(f"Expected \'|\' after child-count at pos {pos}")

    try:
        child_count = int(data[pos:pipe])
    except ValueError:
        raise ValueError(
            f"Non-integer child count \'{data[pos:pipe]}\' at pos {pos}"
        )
    pos = pipe + 1  # consume count + '|'

    children: list[ASTNode] = []
    for _ in range(child_count):
        child, pos = _decode_node(data, pos)
        children.append(child)

    return ASTNode(name=name, value=value, children=children), pos

def deserealize_ast_wire(encoded: str) -> ASTNode:
    """
    Deserealize a flat encoded ASCII string into an ASTNode tree.

    Parameters
    ----------
    encoded:
        The raw encoded string (trailing newline is stripped automatically).

    Returns
    -------
    ASTNode
        Root of the decoded tree.
    """
    encoded = encoded.strip()
    if not encoded:
        raise ValueError("Empty encoded string")

    root, consumed = _decode_node(encoded, 0)

    if consumed != len(encoded):
        raise ValueError(f"Trailing garbage in encoded string starting at pos {consumed}: {encoded[consumed:consumed + 40]!r}")

    return root
