import asyncio
import signal
from typing import Optional

from superggd.base import *



class ClingInstance:
    def __init__(self, path_to_cling, extra_args: Optional[list[str]] = None):
        self.path_to_cling: str = path_to_cling
        self.extra_args = list(extra_args or [])
        self.extra_args.append("--nologo")
        self.instance: Optional[asyncio.subprocess.Process] = None
        self.status = ExecStatus.Exited


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

        if self.instance.returncode is not None:
            self.status = ExecStatus.Exited
            return self.status

        if success_string in line.decode():
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
        return line.decode()

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
