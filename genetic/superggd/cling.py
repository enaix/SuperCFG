import asyncio
from typing import Optional

from superggd.base import *



class ClingInstance:
    def __init__(self, path_to_cling, extra_args: list[str] = []):
        self.path_to_cling: str = path_to_cling
        self.extra_args: list = extra_args
        self.extra_args.append("--nologo")
        self.instance: Optional[asyncio.subprocess.Process] = None
        self.status = ExecStatus.Exited


    async def compile(self, code: str) -> ExecStatus:
        """Compile the code, will exit immediately"""
        self.instance = await asyncio.create_subprocess_exec(self.path_to_cling, *self.extra_args, code, stdin=asyncio.subprocess.PIPE, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE)
        self.status = ExecStatus.Compiling
        return self.status

    async def wait(self, success_string: str) -> ExecStatus:
        """Wait for the compilation"""
        while await self.update_status(success_string) == ExecStatus.Compiling:
            pass
        return self.status

    async def update_status(self, success_string: str) -> ExecStatus:
        """Update and return instance status, will check one stdout line at a time for the matching success substring"""
        assert self.instance is not None, "ClingInstance::update_status() : instance has not been started"
        if self.status != ExecStatus.Compiling:
            return self.status  # nothing to check

        assert self.instance.stdout is not None, "ClingInstance::update_status() : no stdout"
        line = await self.instance.stdout.readline()

        if self.instance.returncode is not None:
            return ExecStatus.Exited

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
    
    async def readline(self) -> str:
        """Read a line from stdout, instance must have the running status"""
        assert self.instance is not None, "ClingInstance::readline() : instance has not been started"
        assert self.status == ExecStatus.Running, "ClingInstance::readline() : instance is not running"
        assert self.instance.stdout is not None, "ClingInstance::readline() : no stdout"

        line = await self.instance.stdout.readline()
        # TODO check if the line is empty
        return line.decode()
