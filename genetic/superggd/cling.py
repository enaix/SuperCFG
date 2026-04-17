import asyncio
import signal
from typing import Optional
import logging

from superggd.base import *


logger = logging.getLogger(__name__)


class ClingInstance:
    def __init__(self, path_to_cling, extra_args: Optional[list[str]] = None):
        self.path_to_cling: str = path_to_cling
        self.extra_args = list(extra_args or [])
        self.extra_args.append("--nologo")
        self.instance: Optional[asyncio.subprocess.Process] = None
        self.status = ExecStatus.Exited
        self.stdout_buffer: str = ""
        self.stderr_buffer: str = ""


    async def compile(self, code: str) -> ExecStatus:
        """Compile the code, will exit immediately"""
        self.instance = await asyncio.create_subprocess_exec(self.path_to_cling, *self.extra_args, stdin=asyncio.subprocess.PIPE, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE)
        self.status = ExecStatus.Compiling
        assert self.instance.stdin is not None, "ClingInstance::compile() : no stdin"
        self.instance.stdin.write((code + "\n").encode())
        await self.instance.stdin.drain()
        return self.status

    async def wait(self, success_string: str) -> ExecStatus:
        """Wait for the compilation"""
        while await self.update_status(success_string) == ExecStatus.Compiling:
            pass
        return self.status

    def is_exited(self) -> bool:
        if self.instance is None:
            return True
        if self.instance.returncode is not None:
            self.status = ExecStatus.Exited
            return True
        return False

    async def update_status(self, success_string: str) -> ExecStatus:
        """Update and return instance status, will check one stdout line at a time for the matching success substring"""
        assert self.instance is not None, "ClingInstance::update_status() : instance has not been started"
        if self.status != ExecStatus.Compiling:
            if self.is_exited():  # updates self.status
                pass
            return self.status  # nothing to check

        assert self.instance.stdout is not None, "ClingInstance::update_status() : no stdout"
        line = await self.instance.stdout.readline()
        if not line:
            self.status = ExecStatus.Exited
            return self.status

        decoded = line.decode()
        self.stdout_buffer += decoded

        if self.instance.returncode is not None:
            self.status = ExecStatus.Exited
            return self.status

        if success_string in decoded:
            # TODO clear stdout & stderr
            self.status = ExecStatus.Running
        return self.status

    async def write_to_stdin(self, line: str) -> None:
        """Write a single line to stdin, instance must have the running status"""
        assert self.instance is not None, "ClingInstance::write_to_stdin() : instance has not been started"
        assert self.status == ExecStatus.Running, "ClingInstance::write_to_stdin() : instance is not running"
        assert self.instance.stdin is not None, "ClingInstance::write_to_stdin() : no stdin"

        self.instance.stdin.write(line.encode())
        await self.instance.stdin.drain()
    
    async def readline(self) -> Optional[str]:
        """Read a line from stdout, instance must have the running status"""
        assert self.instance is not None, "ClingInstance::readline() : instance has not been started"
        assert self.status == ExecStatus.Running, "ClingInstance::readline() : instance is not running"
        assert self.instance.stdout is not None, "ClingInstance::readline() : no stdout"

        line = await self.instance.stdout.readline()
        if not line:  # EOF
            self.status = ExecStatus.Exited
            return None
        decoded = line.decode()
        self.stdout_buffer += decoded
        return decoded

    async def read(self, timeout=0.01) -> str:
        """Drain any currently available stdout without blocking, append to the buffer and return the new chunk. Returns after waiting for input for longer than timeout."""
        assert self.instance is not None, "ClingInstance::read() : instance has not been started"
        assert self.instance.stdout is not None, "ClingInstance::read() : no stdout"

        new_output = await self._read_pipe(self.instance.stdout, timeout)
        self.stdout_buffer += new_output
        return new_output

    async def read_stderr(self, timeout=0.01) -> str:
        """Drain any currently available stderr without blocking, append to the buffer and return the new chunk. Returns after waiting for input for longer than timeout."""
        assert self.instance is not None, "ClingInstance::read() : instance has not been started"
        assert self.instance.stderr is not None, "ClingInstance::read() : no stdout"

        new_output = await self._read_pipe(self.instance.stderr, timeout)
        self.stderr_buffer += new_output
        return new_output

    async def _read_pipe(self, pipe: asyncio.StreamReader, timeout) -> str:
        chunks: list[str] = []
        while True:
            try:
                data = await asyncio.wait_for(pipe.read(4096), timeout=timeout)
            except asyncio.TimeoutError:
                break
            if not data:  # EOF
                self.status = ExecStatus.Exited
                break
            chunks.append(data.decode())
        new_output = "".join(chunks)

        return new_output

    async def read_all(self) -> str:
        """Drain any remaining stdout via read() and return the full accumulated buffer"""
        await self.read()
        return self.stdout_buffer

    async def read_all_stderr(self) -> str:
        """Drain all stderr and return the accumulated buffer"""
        await self.read_stderr()
        return self.stderr_buffer

    def shutdown(self) -> bool:
        if self.instance is None:
            return True
        self.instance.send_signal(signal.SIGINT)
        return self.is_exited()  # else the process is hanging

    def kill(self):
        if self.instance is None:
            return
        self.instance.kill()
        self.status = ExecStatus.Exited
