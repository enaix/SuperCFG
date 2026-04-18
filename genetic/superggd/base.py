from enum import Enum, StrEnum
from typing import Callable, Optional, Any
import os
import sys
import threading
import atexit
import logging
import datetime
import shutil
from typing import Union

from dataclasses import dataclass, field


logger = logging.getLogger(__name__)


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


# AppLogger class
# ===============


@dataclass
class _SolutionLog:
    """Per-solution accumulated execution state within a single generation"""
    stdout: list[str] = field(default_factory=list)
    stderr: list[str] = field(default_factory=list)
    grammar: Optional[str] = None


@dataclass
class _GenerationLog:
    """Per-generation execution state"""
    solutions: dict[int, _SolutionLog] = field(default_factory=dict)
    dumped: bool = False

    def sol(self, idx: int) -> _SolutionLog:
        if idx not in self.solutions:
            self.solutions[idx] = _SolutionLog()
        return self.solutions[idx]


class AppLogger:
    """Singleton execution logger for SuperGGD. Stores per-generation parser stdout/stderr and grammar info, flushes to disk every N generations and on error. Use ``get_applogger()``"""

    _instance: Optional["AppLogger"] = None
    _instance_lock = threading.Lock()

    def __init__(self):
        self._lock = threading.RLock()
        self._configured: bool = False
        self._output_folder: Optional[str] = None
        self._dump_every_n: int = 1
        self._generations: dict[int, _GenerationLog] = {}
        self._current_gen: int = -1
        self._atexit_registered: bool = False
        self._extra_params: dict[str, Any] = {}

    _MARKER_FILE = "superggd.txt"

    def configure(self, output_folder: Optional[str], dump_every_n: int = 1) -> None:
        """Configure the logger. If ``output_folder`` is None, logging is disabled. Will process only the first call"""
        with self._lock:
            if self._configured:
                return  # Handle this only once
            self._output_folder = output_folder
            self._dump_every_n = max(1, int(dump_every_n))
            if self._output_folder is not None:
                self._prepare_output_folder(self._output_folder)
            if not self._atexit_registered:
                atexit.register(self._atexit_dump)
                self._atexit_registered = True
            self._configured = True

    def set_extra_params(self, extra_params: dict[str, Any]) -> None:
        """Update params which will be dumped to superggd.txt"""
        with self._lock:
            self._extra_params = self._extra_params | extra_params

    def start(self) -> None:
        """Write to superggd.txt and start logging"""
        with self._lock:
            if self._output_folder is not None:
                self._write_marker_file(self._output_folder)

    @property
    def enabled(self) -> bool:
        return self._configured and self._output_folder is not None

    def begin_generation(self, gen: int) -> None:
        """Start bucketing log records under the given generation id"""
        if not self.enabled:
            return
        with self._lock:
            self._current_gen = gen
            if gen not in self._generations:
                self._generations[gen] = _GenerationLog()

    def end_generation(self, gen: int) -> None:
        """Mark generation as finished; dumps to disk if the dump cadence matches"""
        if not self.enabled:
            return
        with self._lock:
            if gen % self._dump_every_n == 0:
                self._dump_generation(gen)

    def log_grammar(self, sol_idx: int, grammar_repr: str, gen: Optional[int] = None) -> None:
        """Record grammar representation for a solution within a generation"""
        if not self.enabled:
            return
        with self._lock:
            g = self._gen_bucket(gen)
            g.sol(sol_idx).grammar = grammar_repr

    def log_parser_stdout(self, sol_idx: int, text: str, gen: Optional[int] = None) -> None:
        """Append parser stdout chunk for a solution within a generation"""
        if not self.enabled or not text:
            return
        with self._lock:
            self._gen_bucket(gen).sol(sol_idx).stdout.append(text)

    def log_parser_stderr(self, sol_idx: int, text: str, gen: Optional[int] = None) -> None:
        """Append parser stderr chunk for a solution within a generation"""
        if not self.enabled or not text:
            return
        with self._lock:
            self._gen_bucket(gen).sol(sol_idx).stderr.append(text)

    def save_artifact(self, filename: str, data: Union[str, bytes, bytearray, os.PathLike], sol_idx: Optional[int] = None, gen: Optional[int] = None) -> Optional[str]:
        """
        Write an artifact to ``gen_{gen}/sol_{sol_idx}/{filename}`` if sol_idx exists (or to gen/{gen}/{filename} otherwise)

        ``data`` may be:
        - ``bytes`` / ``bytearray``: written as binary
        - ``os.PathLike``: treated as a source file path, copied into place
        - ``str``: if it points to an existing file, copied; otherwise written as text

        Returns the destination path or None if logging is disabled
        """
        if not self.enabled:
            return None
        if gen is None:
            gen = self._current_gen
        assert self._output_folder is not None, "AppLogger::save_artifact() : output_folder is None"

        # Prevent directory traversal
        safe_name = os.path.basename(filename)
        if not safe_name or safe_name in (".", ".."):
            raise ValueError(f"AppLogger::save_artifact() : invalid filename {filename!r}")

        if sol_idx is not None:
            sol_dir = os.path.join(self._output_folder, f"gen_{gen}", f"sol_{sol_idx}")
            dest = os.path.join(sol_dir, safe_name)
        else:
            sol_dir = None
            dest = os.path.join(self._output_folder, safe_name)
        try:
            with self._lock:
                if sol_dir:
                    os.makedirs(sol_dir, exist_ok=True)
                if isinstance(data, (bytes, bytearray)):
                    with open(dest, "wb") as f:
                        f.write(data)
                elif isinstance(data, os.PathLike):
                    shutil.copyfile(os.fspath(data), dest)
                elif isinstance(data, str):
                    if os.path.isfile(data):
                        shutil.copyfile(data, dest)
                    else:
                        with open(dest, "w", encoding="utf-8") as f:
                            f.write(data)
                else:
                    raise TypeError(f"AppLogger::save_artifact() : data must be str, bytes or PathLike, got {type(data).__name__}")
        except Exception as e:
            logger.exception(f"AppLogger::save_artifact() : failed to save {safe_name!r} for sol_{sol_idx} gen {gen} : {e}")
            return None
        return dest

    def dump_all(self) -> None:
        """Flush all in-memory generations to disk. Called on error / exit"""
        if not self.enabled:
            return
        with self._lock:
            for gen in list(self._generations.keys()):
                bucket = self._generations[gen]
                if bucket.dumped and not bucket.solutions:
                    continue  # already flushed and cleared
                try:
                    self._dump_generation(gen)
                except Exception as e:
                    logger.exception(f"AppLogger::dump_all() : failed to dump generation {gen} : {e}")

    # Internals
    # =========

    def _prepare_output_folder(self, folder: str) -> None:
        """Ensure the output folder is safe to write into. If it already contains the marker file, archive the old folder to ``folder_DDMMYYYY_i``; if it exists without the marker, raise an error"""
        if not os.path.exists(folder):
            os.makedirs(folder, exist_ok=True)
            return
        if not os.path.isdir(folder):
            raise ValueError(f"AppLogger::configure() : output_folder {folder!r} exists but is not a directory")

        marker = os.path.join(folder, self._MARKER_FILE)
        if os.path.isfile(marker):
            date_str = datetime.datetime.now().strftime("%d%m%Y")
            archive = f"{folder}_{date_str}"
            # Find a free suffix if the dated folder already exists
            candidate = archive
            i = 1
            while os.path.exists(candidate):
                candidate = f"{archive}_{i}"
                i += 1
            os.rename(folder, candidate)
            print(f"AppLogger: existing output folder archived to {candidate}", file=sys.stderr)
            os.makedirs(folder, exist_ok=True)
        else:
            raise ValueError(f"AppLogger::configure() : output_folder {folder!r} already exists and is not a SuperGGD output folder (no {self._MARKER_FILE}). Refusing to write into it")

    def _write_marker_file(self, folder: str) -> None:
        """Write the superggd.txt marker with the creation date and execution parameters"""
        params: dict[str, Any] = {
            "output_folder": folder,
            "dump_every_n": self._dump_every_n,
        }
        if self._extra_params:
            params.update(self._extra_params)
        path = os.path.join(folder, self._MARKER_FILE)
        with open(path, "w", encoding="utf-8") as f:
            f.write(f"created: {datetime.datetime.now().isoformat(timespec='seconds')}\n")
            f.write("parameters:\n")
            for k, v in params.items():
                f.write(f"  {k} = {v!r}\n")

    def _gen_bucket(self, gen: Optional[int]) -> _GenerationLog:
        if gen is None:
            gen = self._current_gen
        if gen not in self._generations:
            self._generations[gen] = _GenerationLog()
        return self._generations[gen]

    def _dump_generation(self, gen: int) -> None:
        assert self._output_folder is not None, "AppLogger::_dump_generation() : output_folder is None"
        logger.debug("AppLogger::_dump_generation() : begin...")
        bucket = self._generations.get(gen)
        if bucket is None:
            return
        gen_dir = os.path.join(self._output_folder, f"gen_{gen}")
        os.makedirs(gen_dir, exist_ok=True)

        # Per-solution grammar / stdout / stderr
        fully_dumped = True
        for sol_idx, sol in bucket.solutions.items():
            sol_dir = os.path.join(gen_dir, f"sol_{sol_idx}")
            try:
                os.makedirs(sol_dir, exist_ok=True)
                if sol.grammar is not None:
                    with open(os.path.join(sol_dir, "grammar.txt"), "w", encoding="utf-8") as f:
                        f.write(sol.grammar)
                if sol.stdout:
                    with open(os.path.join(sol_dir, "stdout.txt"), "w", encoding="utf-8") as f:
                        f.write("".join(sol.stdout))
                if sol.stderr:
                    with open(os.path.join(sol_dir, "stderr.txt"), "w", encoding="utf-8") as f:
                        f.write("".join(sol.stderr))
            except Exception as e:
                fully_dumped = False
                logger.exception(f"AppLogger::_dump_generation() : failed to dump sol_{sol_idx} for gen {gen} : {e}")

        bucket.dumped = True
        # Release the in-memory buffers for this generation to avoid heap growth across long runs.
        # Keep the bucket marker (dumped=True) so repeated dump_all() calls become no-ops
        if fully_dumped:
            bucket.solutions.clear()

    def _atexit_dump(self) -> None:
        try:
            self.dump_all()
        except Exception as e:
            logger.exception(f"AppLogger::_atexit_dump() raised : {e}")


def get_applogger() -> AppLogger:
    """Return the AppLogger singleton"""
    if AppLogger._instance is None:
        with AppLogger._instance_lock:
            if AppLogger._instance is None:
                AppLogger._instance = AppLogger()
    return AppLogger._instance
