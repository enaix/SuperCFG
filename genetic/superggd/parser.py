import os
import concurrent.futures
import asyncio
import threading
from typing import Optional, Any

from superggd.cling import ClingInstance
from superggd.operators import *
from superggd.base import *

import logging
logger = logging.getLogger(__name__)


SUPERCFG_SOURCE_SINGLE = """
#include <iostream>

#include "cfg/gbnf.h"
#include "cfg/containers.h"
#include "cfg/base.h"
#include "cfg/parser.h"
#include "cfg/str.h"
#include "cfg/preprocess_factories.h"


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
    VStr input;
    std::cout << "SUPERCFG_READY" << std::endl;

    std::cin >> input;

    // TODO split input into command and data

    bool ok;

    // Tokenize the input
    auto tokens = lexer.run(input, ok);

    if (!ok) {
        std::cout << "SUPERCFG_FAIL 0 " << seq << std::endl;
        continue;
    }

    // Create a parse tree
    TreeNode<VStr> tree;

    ok = parser.run(tree, json, tokens);

    if (!ok) {
        std::cout << "main() : parser failed" << std::endl;
        return 1;
    }

    // Process the parse tree
    // TODO make a reliable interface for encoding AST
    tree.traverse([&](const auto& node, std::size_t depth) {
        // Print the tree structure
        for (std::size_t i = 0; i < depth; i++)
            std::cout << "|  ";
        std::cout << node.name << " (" << node.nodes.size()
                  << " elems) : " << node.value << std::endl;
    });

}
"""


class SuperCFGParser:
    def __init__(self, path_to_cling: str = "cling", path_to_supercfg: str = "../", extra_cling_args: list[str] = []):
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

    async def run(self, string: str) -> Optional[str]:
        if self.cling.status != ExecStatus.Running:
            return None

        self.cur_seq += 1
        # TODO consume all stdout first, so that we process only parsing output
        await self.cling.write_to_stdin(string)
        output = await self.cling.readline()

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
            return out[1]  # TODO convert to ast
        elif out[0] == "SUPERCFG_FAIL":
            return None  # parsing failed
        else:
            # TODO log errors
            logger.error("bad SuperCFG output : no such command : %s", output)
            return None  # bad string
       
    def to_ebnf_repr(self, grammar: Grammar) -> str:
        return grammar.bake_supercfg()  # the method actually exists


class ParserInstance:
    def __init__(self, grammar: Grammar, parser: Any, compilation_strategy: CompilationStrategy):
        self.parser = parser
        self.grammar: Optional[Grammar] = grammar
        self.comp_strategy = compilation_strategy

    async def compile(self) -> ExecStatus:
        status = await self.parser.compile(self.grammar)
        if status == ExecStatus.Exited:
            if self.comp_strategy == CompilationStrategy.Die:
                return status
            # We do not have any more strategies yet
        return status

    def status(self) -> ExecStatus:
        return self.parser.status


class ParserManager:
    num_parallel: int

    _loop: Optional[asyncio.AbstractEventLoop] = None
    _thread: Optional[threading.Thread] = None
    _lock = threading.Lock()

    def __init__(self, num_parallel: int, compilation_strategy: CompilationStrategy = CompilationStrategy.Die):
        self.num_parallel = num_parallel
        self.futures: list[concurrent.futures.Future] = []
        self.instances: list[ParserInstance] = []
        self.comp_strategy = compilation_strategy

    def _get_loop(self) -> asyncio.AbstractEventLoop:
        """Create a new event loop for parsing events"""
        global _loop, _thread
        with self._lock:
            if self._loop is None or not self._loop.is_running():
                self._loop = asyncio.new_event_loop()
                self._thread = threading.Thread(
                    target=self._loop.run_forever, daemon=True, name="superggd-event-loop"
                )
                self._thread.start()
        return self._loop

    def submit(self, coro) -> concurrent.futures.Future:
        """Submit a coroutine to the background loop, returns Future (sync-safe)"""
        return asyncio.run_coroutine_threadsafe(coro, self._get_loop())

    def compile_batch(self, grammars: list[Grammar], parsers: list[Any]) -> None:
        """Compile a set of parsers. Starts a background job, see status()"""
        for i in range(len(parsers)):
            #parsers[i].compile(grammars[i])
            self.instances[i] = ParserInstance(grammars[i], parsers[i], self.comp_strategy)
            self.futures[i] = self.submit(self.instances[i].compile())

    def status(self) -> list[ExecStatus]:
        """Check execution status"""
        res: list[ExecStatus] = []
        for i in range(len(self.futures)):
            if self.futures[i].done():
                res[i] = self.instances[i].status() # Should be thread-safe, since the compilation is completed
            else:
                res[i] = ExecStatus.Compiling
        return res

    def is_done(self):
        return ExecStatus.Compiling not in self.status()
