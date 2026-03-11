from enum import Enum, StrEnum

from dataclasses import dataclass, field


class ExecStatus(Enum):
    """Basic execution status for a program"""
    Compiling = 0,
    Running = 1,
    Exited = 2  # Indicates compilation error for a JIT compiler


class CompilationStrategy(StrEnum):
    """How to handle compilation errors"""
    Die = "die",


@dataclass
class ASTNode:
    """Abstract syntax tree node"""
    name:     str
    value:    str
    children: list[ASTNode] = field(default_factory=list)

    # ------------------------------------------------------------------
    def pformat(self, _depth: int = 0) -> str:
        """Return a human-readable representation"""
        indent = "|  " * _depth
        header = f"{indent}{self.name!r} ({len(self.children)} children) : {self.value!r}"
        child_lines = [c.pformat(_depth + 1) for c in self.children]
        return "\n".join([header] + child_lines)

    def __repr__(self) -> str:
        return (f"ASTNode(name={self.name!r}, value={self.value!r}, children=[{len(self.children)} nodes])")


# Wire ast representation deserealizer
# ====================================

class DecodeError(ValueError):
    """Raised when the encoded string is malformed."""


def _decode_field(data: str, pos: int) -> tuple[str, int]:
    """
    Parse one <decimal_len>:<hex_body> field starting at pos.

    Returns
    -------
    (decoded_string, new_pos)
    """
    colon = data.find(':', pos)
    if colon == -1:
        raise DecodeError(f"Expected \':\' after byte-length at pos {pos}")

    try:
        byte_len = int(data[pos:colon])
    except ValueError:
        raise DecodeError(
            f"Non-integer byte-length \'{data[pos:colon]}\' at pos {pos}"
        )

    hex_start = colon + 1
    hex_end   = hex_start + byte_len * 2  # 2 hex digits per byte

    if hex_end > len(data):
        raise DecodeError(
            f"Hex body truncated: need {byte_len * 2} chars "
            f"at pos {hex_start}, but string ends at {len(data)}"
        )

    hex_body = data[hex_start:hex_end]
    try:
        decoded = bytes.fromhex(hex_body).decode("utf-8")
    except (ValueError, UnicodeDecodeError) as exc:
        raise DecodeError(f"Bad hex body at pos {hex_start}: {exc}") from exc

    return decoded, hex_end


def _decode_node(data: str, pos: int) -> tuple[ASTNode, int]:
    """
    Parse one node (and all its descendants) starting at pos.

    Returns
    -------
    (ASTNode, new_pos)
    """
    # --- name ---
    name, pos = _decode_field(data, pos)

    if pos >= len(data) or data[pos] != '|':
        raise DecodeError(f"Expected \'|\' after name field at pos {pos}")
    pos += 1  # consume '|'

    # --- value ---
    value, pos = _decode_field(data, pos)

    if pos >= len(data) or data[pos] != '|':
        raise DecodeError(f"Expected '|' after value field at pos {pos}")
    pos += 1  # consume '|'

    # --- child count ---
    pipe = data.find('|', pos)
    if pipe == -1:
        raise DecodeError(f"Expected \'|\' after child-count at pos {pos}")

    try:
        child_count = int(data[pos:pipe])
    except ValueError:
        raise DecodeError(
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
    Deserealize a flat encoded string into an ASTNode tree.

    Parameters
    ----------
    encoded:
        The raw encoded string (trailing newline is stripped automatically).

    Returns
    -------
    ASTNode
        Root of the decoded tree.

    Raises
    ------
    DecodeError
        If the string is malformed.  Callers that want to be resilient can
        catch `ValueError` (the base class of `DecodeError`).
    """
    encoded = encoded.strip()
    if not encoded:
        raise DecodeError("Empty encoded string")

    root, consumed = _decode_node(encoded, 0)

    if consumed != len(encoded):
        raise DecodeError(
            f"Trailing garbage in encoded string starting at pos {consumed}: "
            f"{encoded[consumed:consumed + 40]!r}"
        )

    return root
