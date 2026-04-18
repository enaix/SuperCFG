import os
import concurrent.futures
import asyncio
import threading
import time
from typing import Optional, Any

from superggd.operators import *
from superggd.base import *

import logging
logger = logging.getLogger(__name__)






class ParserInstance:
    def __init__(self, grammar: Grammar, parser: Any, compilation_strategy: CompilationStrategy):
        self.parser = parser
        self.grammar: Grammar = grammar
        self.future: Optional[concurrent.futures.Future] = None
        self.comp_strategy = compilation_strategy

    async def compile(self) -> ExecStatus:
        status = await self.parser.compile(self.grammar)
        if status == ExecStatus.Exited:
            if self.comp_strategy == CompilationStrategy.Die:
                raise RuntimeError("Parser generator has failed, stopping execution...")
            elif self.comp_strategy == CompilationStrategy.Skip:
                return status
            # We do not have any more strategies yet
        return status

    def status(self) -> ExecStatus:
        return self.parser.status()


class ParserManager:
    def __init__(self, num_parallel: int, compilation_strategy: CompilationStrategy = CompilationStrategy.Die):
        self.num_parallel = num_parallel
        self.instances: dict[Grammar, ParserInstance] = {}
        self.comp_strategy = compilation_strategy
        self._loop: Optional[asyncio.AbstractEventLoop] = None
        self._thread: Optional[threading.Thread] = None
        self._lock = threading.Lock()

    def _get_loop(self) -> asyncio.AbstractEventLoop:
        """Create a new event loop for parsing events"""
        with self._lock:
            if self._loop is None or not self._loop.is_running():
                self._loop = asyncio.new_event_loop()
                self._thread = threading.Thread(target=self._loop.run_forever, daemon=True, name="superggd-event-loop")
                self._thread.start()
        return self._loop

    def submit(self, coro) -> concurrent.futures.Future:
        """Submit a coroutine to the background loop, returns Future (sync-safe)"""
        return asyncio.run_coroutine_threadsafe(coro, self._get_loop())

    def reset(self) -> None:
        for inst in self.instances.values():
            if inst.future and not inst.future.done():
                inst.future.cancel()

        for g in self.instances.keys():
            self.instances[g].parser.shutdown()
        self.instances.clear()

    def compile_batch(self, grammars: list[Grammar], parsers: list[Any]) -> None:
        """Compile a set of parsers. Starts a background job, see status()"""
        self.reset()
        for i in range(len(parsers)):
            inst = ParserInstance(grammars[i], parsers[i], self.comp_strategy)
            inst.future = self.submit(inst.compile())
            self.instances[grammars[i]] = inst

    def run(self, grammar: Grammar, input_string: str, timeout: Optional[int] = None) -> tuple[bool, Optional[ASTNode]]:
        if grammar not in self.instances:
            logger.error("ParserManager::run() : no parser instance for grammar")
            return False, None  # No such grammar
        status = self.instances[grammar].status()
        if status == ExecStatus.Compiling:
            logger.error("ParserManager::run() : parser is still compiling")
            return False, None
        elif status == ExecStatus.Compiling:
            logger.error("ParserManager::run() : parser exited")
            return False, None

        future = self.submit(self.instances[grammar].parser.run(input_string))
        # TODO properly log everything, since we need to know which input & grammar caused the error
        return future.result(timeout=timeout)

    def status(self) -> dict[Grammar, ExecStatus]:
        """Check compilation status"""
        res: dict[Grammar, ExecStatus] = {}
        for g in self.instances.keys():
            inst = self.instances[g]
            assert inst.future is not None, "ParserManager::status() : there is no future"
            if inst.future.done():
                res[g] = inst.status()  # Should be thread-safe, since the compilation is completed
            else:
                res[g] = ExecStatus.Compiling
        return res

    def wait(self, timeout: Optional[float] = None) -> dict[Grammar, ExecStatus]:
        """
        Block the calling thread until all pending compilations finish. Timeout is specified in fractional seconds. Returns the final status of each grammar.
        Raises concurrent.futures.TimeoutError if timeout is exceeded.
        """
        if not self.instances:
            return {}

        deadline = (time.monotonic() + timeout) if timeout is not None else None

        for g, inst in self.instances.items():
            if inst.future is None:
                continue

            remaining = (deadline - time.monotonic()) if deadline is not None else None
            if remaining is not None and remaining <= 0:
                raise concurrent.futures.TimeoutError(f"ParserManager::wait() timed out before compiling grammar: {g}")

            try:
                inst.future.result(timeout=remaining)
            except concurrent.futures.TimeoutError:
                raise concurrent.futures.TimeoutError(f"ParserManager::wait() timed out while compiling grammar: {g}")
            #except Exception as e:
            #    logger.error("ParserManager::wait() : compilation of grammar %s raised: %s", g, e)

        return self.status()

    def is_done(self):
        return ExecStatus.Compiling not in self.status().values()
