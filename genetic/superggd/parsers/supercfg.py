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
    VStr input, tok, parsed_tok;
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
            }
            try {
                parsed_tok = cfg_helpers::hex_decode(tok);
            } catch (std::invalid_argument e) {
                std::cout << "SUPERCFG_FAIL BAD_INPUT " << seq << std::endl;
                continue;
            }
            // run the default loop
        } else {
            std::cout << "SUPERCFG_FAIL BAD_CMD " << seq << std::endl;
            continue;
        }
    }

    bool ok;

    // Tokenize the input
    auto tokens = lexer.run(parsed_tok, ok);

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
        """Initialize SuperCFG parser generator. Extra arguments are ignored"""
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
        try:
            string_enc = encode_input_hex(string)
        except ValueError as e:
            logger.error("Input string is not ASCII")
            return None
        await self.cling.write_to_stdin(("SUPERCFG_PARSE " + string_enc + "\n"))
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
