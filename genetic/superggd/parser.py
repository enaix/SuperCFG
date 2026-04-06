import os
import concurrent.futures
import asyncio
import threading
import time
from typing import Optional, Any

from superggd.cling import ClingInstance
from superggd.operators import *
from superggd.base import *

import logging
logger = logging.getLogger(__name__)


SUPERCFG_SOURCE_SINGLE = """
#include <iostream>
#include <sstream>

#include "cfg/gbnf.h"
#include "cfg/containers.h"
#include "cfg/base.h"
#include "cfg/parser.h"
#include "cfg/str.h"
#include "cfg/preprocess_factories.h"
#include "extra/ast_serializer.h"


SUPERCFG_RULESET_DEF


constexpr auto conf = mk_sr_parser_conf<
    SRConfEnum::PrettyPrint,  // Enable pretty printing for debugging
    SRConfEnum::Lookahead,     // Enable lookahead(1)
    SRConfEnum::HeuristicCtx>(); // Enable HeurCtx


auto lexer = make_lexer<VStr, TokenType>(ruleset, mk_lexer_conf<
    LexerConfEnum::AdvancedLexer,    // Enable advanced lexer
    LexerConfEnum::HandleDuplicates // Handle duplicate tokens
    >());


auto parser = make_sr_parser<VStr, TokenType, TreeNode<VStr>>(ruleset, lexer, conf, printer);

for (std::size_t seq = 1; ; seq++)
{
    VStr input, tok;
    std::cout << "SUPERCFG_READY" << std::endl;

    std::cin >> input;

    std::stringstream ss(input);
    if (!std::getline(ss, tok, ' '))
    {
        std::cout << "SUPERCFG_FAIL BAD_INPUT " << seq << std::endl;
        continue;
    }

    if (tok == "SUPERCFG_EXIT") {
        std::cout << "SUPERCFG_OK EXIT " << seq << std::endl;
        return 0;
    } else {
        if (tok == "SUPERCFG_PARSE") {
            if (!std::getline(ss, tok, ' ')) {
                std::cout << "SUPERCFG_FAIL BAD_INPUT " << seq << std::endl;
                continue;
            } // else run the default loop
        } else {
            std::cout << "SUPERCFG_FAIL BAD_CMD " << seq << std::endl;
            continue;
        }
    }

    bool ok;

    // Tokenize the input
    auto tokens = lexer.run(tok, ok);

    if (!ok) {
        std::cout << "SUPERCFG_FAIL TOKENIZER_FAIL " << seq << std::endl;
        continue;
    }

    // Create a parse tree
    TreeNode<VStr> tree;

    ok = parser.run(tree, json, tokens);

    if (!ok) {
        std::cout << "SUPERCFG_FAIL PARSER_FAIL " << seq << std::endl;
        continue;
    }

    // Process the parse tree
    std::cout << "SUPERCFG_OK " << serialize_ast_wire<VStr, TreeNode<VStr>>(tree) << " " << seq << std::endl;
}
"""


class SuperCFGParser:
    def __init__(self, path_to_cling: str = "cling", path_to_supercfg: str = "../", extra_cling_args: list[str] = [], **kwargs):
        """Initialize SuperCFG parser generators. Extra arguments are ignored"""
        self.cling = ClingInstance(path_to_cling, ["-I" + path_to_supercfg] + extra_cling_args)
        self.success_string = "SUPERCFG_READY"
        self.cur_seq = 0 # sequence number of the execution

    async def compile(self, grammar: Grammar) -> ExecStatus:
        """Compile and wait for the completion"""
        ebnf = self.to_ebnf_repr(grammar)
        code = SUPERCFG_SOURCE_SINGLE.replace("SUPERCFG_RULESET_DEF", ebnf)

        await self.cling.compile(code)
        return await self.cling.wait(self.success_string)

    def status(self) -> ExecStatus:
        return self.cling.status  # Seems to be safe

    async def run(self, string: str) -> Optional[ASTNode]:
        if self.cling.status != ExecStatus.Running:
            return None

        self.cur_seq += 1
        # TODO consume all stdout first, so that we process only parsing output
        await self.cling.write_to_stdin(("SUPERCFG_PARSE " + string + "\n"))
        if self.cling.is_exited():
            logger.error("SuperCFG has unexpectedly exited")
            return None  # we should handle stdout here
        output = await self.cling.readline()
        if output is None:
            logger.error("SuperCFG has unexpectedly exited")
            return None

        out = output.split(' ')
        if len(out) != 3:
            logger.error("bad SuperCFG output : bad split : %s", output)
            return None  # bad string format

        try:
            seq = int(out[2])
        except ValueError:
            logger.error("could not parse SuperCFG output : bad seq : %s", output)
            return None

        if seq != self.cur_seq:
            logger.error("bad SuperCFG output : seq num does not match : %s", output)
            # Try to continue parsing anyways

        if out[0] == "SUPERCFG_OK":
            try:
                ast = deserealize_ast_wire(out[1])
            except ValueError as e:
                logger.error(f"bad SuperCFG output : {e}")
                return None  # bad format
            return ast

        elif out[0] == "SUPERCFG_FAIL":
            logger.info("SuperCFG failed, reason : %s", out[1])
            return None  # parsing failed
        else:
            logger.error("bad SuperCFG output : no such command : %s", output)
            return None  # bad string

    def shutdown(self):
        if self.cling.is_exited():
            return
        if not self.cling.shutdown():
            self.cling.kill()

       
    def to_ebnf_repr(self, grammar: Grammar) -> str:
        return grammar.bake_supercfg()  # the method actually exists


# Main parser generator classes
SUPERGGD_PARSER_GENERATORS = {"supercfg": SuperCFGParser}



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
        return self.parser.status


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

    def run(self, grammar: Grammar, input_string: str, timeout: Optional[int] = None) -> Optional[ASTNode]:
        if grammar not in self.instances:
            logger.error("ParserManager::run() : no parser instance for grammar")
            return None  # No such grammar
        if self.instances[grammar].status() != ExecStatus.Compiling:
            logger.error("ParserManager::run() : parser is still compiling")
            return None

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
