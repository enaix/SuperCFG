from typing import Optional, Any
from enum import Flag, auto

from superggd.cling import ClingInstance
from superggd.operators import *
from superggd.base import *


import logging
logger = logging.getLogger(__name__)


class SRConfEnum(Flag):
    EmptyFlag = auto()  # Used for explicitly settings an empty flag (None is used as default value instead)
    Lookahead = auto()
    HeuristicCtx = auto()


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
SUPERCFG_SR_PARSER_CONF
    >();


auto lexer = make_lexer<VStr, TokenType>(ruleset, mk_lexer_conf<
    LexerConfEnum::AdvancedLexer,
    LexerConfEnum::HandleDuplicates
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
        if (tree.nodes.size() > 0 || tree.name.size() > 0)
            std::cout << "SUPERCFG_FAIL " << serialize_ast_wire<VStr, TreeNode<VStr>>(tree) << " " << seq << std::endl;
        else
            std::cout << "SUPERCFG_FAIL PARSER_FAIL " << seq << std::endl;
        continue;
    }

    // Process the parse tree
    std::cout << "SUPERCFG_OK " << serialize_ast_wire<VStr, TreeNode<VStr>>(tree) << " " << seq << std::endl;
}
"""


class SuperCFGParser:
    def __init__(self, solution_idx: int, path_to_cling: str = "cling", path_to_supercfg: str = "../", supercfg_args: Optional[SRConfEnum] = None, extra_cling_args: list[str] = [], **kwargs):
        """Initialize SuperCFG parser generator. Extra arguments are ignored"""
        self.solution_idx = solution_idx
        self.cling = ClingInstance(path_to_cling, ["-I" + path_to_supercfg] + extra_cling_args)
        self.success_string = "SUPERCFG_READY"
        if supercfg_args is None:
            self.supercfg_conf = SRConfEnum.Lookahead | SRConfEnum.HeuristicCtx
        else:
            self.supercfg_conf = supercfg_args
        self.cur_seq = 0 # sequence number of the execution

    async def compile(self, grammar: Grammar) -> ExecStatus:
        """Compile and wait for the completion"""
        logger.debug(f"compile() : [{self.solution_idx}] init ...")
        ebnf = self.to_ebnf_repr(grammar)
        parser_enum: list[str] = []
        if SRConfEnum.Lookahead in self.supercfg_conf:
            parser_enum.append("SRConfEnum::Lookahead")
        if SRConfEnum.HeuristicCtx in self.supercfg_conf:
            parser_enum.append("SRConfEnum::HeuristicCtx")
        code = SUPERCFG_SOURCE_SINGLE.replace("SUPERCFG_RULESET_DEF", ebnf).replace("SUPERCFG_SR_PARSER_CONF", ',\n'.join(parser_enum))

        await self.cling.compile(code)
        status = await self.cling.wait(self.success_string)
        if status != ExecStatus.Running:
            logger.warning(f"compile() : [{self.solution_idx}] compilation failed")
        return status

    def status(self) -> ExecStatus:
        return self.cling.status  # Seems to be safe

    async def run(self, string: str) -> tuple[bool, Optional[ASTNode]]:
        if self.cling.status != ExecStatus.Running:
            return False, None

        self.cur_seq += 1
        # TODO consume all stdout first, so that we process only parsing output
        try:
            string_enc = encode_input_hex(string)
        except ValueError as e:
            logger.error(f"run() : [{self.solution_idx}] Input string is not ASCII")
            return False, None
        await self.cling.write_to_stdin(("SUPERCFG_PARSE " + string_enc + "\n"))
        if self.cling.is_exited():
            logger.error(f"run() : [{self.solution_idx}] SuperCFG has unexpectedly exited")
            return False, None  # we should handle stdout here
        output = await self.cling.readline()
        if output is None:
            logger.error(f"run() : [{self.solution_idx}] SuperCFG has unexpectedly exited")
            return False, None

        out = output.split(' ')
        if len(out) != 3:
            logger.error(f"run() : [{self.solution_idx}] bad SuperCFG output : bad split : %s", output)
            return False, None  # bad string format

        try:
            seq = int(out[2])
        except ValueError:
            logger.error(f"run() : [{self.solution_idx}] could not parse SuperCFG output : bad seq : %s", output)
            return False, None

        if seq != self.cur_seq:
            logger.error(f"run() : [{self.solution_idx}] bad SuperCFG output : seq num does not match : %s", output)
            # Try to continue parsing anyways

        if out[0] == "SUPERCFG_OK":
            try:
                ast = deserealize_ast_wire(out[1])
            except ValueError as e:
                logger.error(f"run() : [{self.solution_idx}] bad SuperCFG output : {e}")
                return False, None  # bad format
            return True, ast

        elif out[0] == "SUPERCFG_FAIL":
            if out[1] == "TOKENIZER_FAIL":
                logger.info(f"run() : [{self.solution_idx}] SuperCFG failed, reason : tokenizer failure")
                return False, None
            elif out[1] == "PARSER_FAIL":
                logger.info(f"run() : [{self.solution_idx}] SuperCFG failed, reason : parser failure, no AST produced")
                return False, None
            else:
                try:
                    ast = deserealize_ast_wire(out[1])
                except ValueError as e:
                    logger.info(f"run() : [{self.solution_idx}] SuperCFG failed, could not decode AST : %s", out[1])
                    return False, None  # bad format
                logger.info(f"run() : [{self.solution_idx}] SuperCFG failed, returning partial result")
                return False, ast  # parsing failed, return partial ast
        else:
            logger.error(f"run() : [{self.solution_idx}] bad SuperCFG output : no such command : %s", output)
            return False, None  # bad string

    def shutdown(self):
        if self.cling.is_exited():
            return
        if not self.cling.shutdown():
            self.cling.kill()


    def to_ebnf_repr(self, grammar: Grammar) -> str:
        return grammar.bake_supercfg()  # the method actually exists
