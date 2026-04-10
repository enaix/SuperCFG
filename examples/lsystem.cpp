#include <iostream>
#include <chrono>

#include "cfg/gbnf.h"
#include "cfg/str.h"
#include "cfg/base.h"
#include "cfg/parser.h"
#include "cfg/containers.h"

#include "extra/superdbg.h"


int main()
{
    // Binary tree
    constexpr auto zero = NTerm(cs<"zero">());
    constexpr auto one = NTerm(cs<"one">());
    constexpr auto d_zero = Define(zero, Alter(Term(cs<"0">()), Concat(one, Term(cs<"[">()), zero, Term(cs<"]">()), zero)));
    constexpr auto d_one = Define(one, Alter(Term(cs<"1">()), Concat(one, one)));
    constexpr auto ruleset = RulesDef(d_zero, d_one);

    // Target string: 1111[11[1[0]0]1[0]0]11[1[0]0]1[0]0
    // Common subsequences of len 2 (with step over):
    // 11 : 4 (store inclusion positions)
    // 1[ : 7 - *1
    // [1 : 4
    // ]1 : 3
    // [0; 0]; - *1
    // *1 - set of mut. excl. positions
    // Strong condition: two substrings are mut.excl iff \exists s1={i1, j1}, s2={i2, j2}, s1\ne s2, |s1|<|s2|: i1>=i2 && j1<=j2
    //
    // Grammar generator: need to obtain 2 rules
    // Deduce the list of variables and constants, supposedly randomly
    //
    // pick two longest repeating strings
    //
    /*

    Code with step-over:
def r2(s):
    pr = {}
    sls = {}
    all_subs = [] # should be an orderedset
    for l in range(1, len(s)):
        for start in range(0, len(s) - 1):
            # Get all possible substrings
            sub = s[start:start + l]
            if sub not in all_subs:
                all_subs.append(sub)

    for sub in all_subs:
        # scan for the positions:
        start = 0
        while start < (len(s) - len(sub) + 1):
            if s[start:start + len(sub)] == sub:
                if pr.get(sub) is not None:
                    pr[sub] += 1
                    sls[sub].append((start, start + len(sub)))
                else:
                    pr[sub] = 1
                    sls[sub] = [(start, start + len(sub))]
                start += len(sub) # skip over
            else:
                start += 1
    return pr, sls

    Find strings which are strongly mut.excl. with each other:

def mut_strong(pr, sls):
    mut = {}
    cbi = {}  # cannot be in
    for i, sub in enumerate(pr.keys()):
        for j, s2 in enumerate(pr.keys()):
            if j == i or (not sub in s2) or len(sls[sub]) == 0 or len(sls[sub]) != len(sls[s2]):
                continue
            # also skip subs of size 1
            if pr[sub] == 1 or pr[s2] == 1:
                continue
            # Check strong condition
            # We assume that sls is sorted
            for k in range(len(sls[s2])):
                if not (sls[sub][k][0] >= sls[s2][k][0] and sls[sub][k][1] <= sls[s2][k][1]):
                    continue
            if mut.get(s2) is not None:
                mut[s2].append(sub)
            else:
                mut[s2] = [sub]
            if cbi.get(sub) is not None:
                cbi[sub].append(s2)
            else:
                cbi[sub] = [s2]
    return mut, cbi



    BROKEN:
def r(s):
    pr = {}
    for l in range(1, len(s)):
        for start in range(0, len(s) - 1, l):
            if start + l >= len(s):
                break
            sub = s[start:start + l]
            if sub in s:
                if pr.get(sub) is not None:
                    pr[sub] += 1
                else:
                    pr[sub] = 1
    return pr


     */

    using VStr = StdStr<char>; // Variable string class inherited from std::string<TChar>
    using TokenType = StdStr<char>; // Class used for storing a token type in runtime

    // Configure the parser with desired options
    constexpr auto conf = mk_sr_parser_conf<
        SRConfEnum::PrettyPrint  // Enable pretty printing for debugging
        //SRConfEnum::Lookahead// Enable lookahead(1)
        >();//SRConfEnum::HeuristicCtx>();

    // Initialize the tokenizer
    auto lexer = make_lexer<VStr, TokenType>(ruleset, mk_lexer_conf<LexerConfEnum::AdvancedLexer, LexerConfEnum::HandleDuplicates>());

    DBGPrinter printer;
    printer.init_signal_handler();

    // Create the shift-reduce parser
    // TreeNode<VStr> is the AST class
    auto parser = make_sr_parser<VStr, TokenType, TreeNode<VStr>>(ruleset, lexer, conf, printer);

    while(true)
    {
        VStr input;
        std::cout << "lsys> ";
        std::cout.flush();
        std::cin >> input;

        bool ok;

        // Tokenize the input
        volatile std::chrono::steady_clock::time_point lex_start = std::chrono::steady_clock::now();
        auto tokens = lexer.run(input, ok);
        volatile std::chrono::steady_clock::time_point lex_end = std::chrono::steady_clock::now();

        if (!ok) {
            std::cout << "main() : lexer failed" << std::endl;
            return 1;
        }

        // Create a parse tree
        TreeNode<VStr> tree;

        volatile std::chrono::steady_clock::time_point p_start = std::chrono::steady_clock::now();
        ok = parser.run(tree, zero, tokens, printer);
        volatile std::chrono::steady_clock::time_point p_end = std::chrono::steady_clock::now();

        if (!ok) {
            std::cout << "main() : parser failed" << std::endl;
            return 1;
        }

        // Process the parse tree
        tree.traverse([&](const auto& node, std::size_t depth) {
            // Print the tree structure
            for (std::size_t i = 0; i < depth; i++)
                std::cout << "|  ";
            std::cout << node.name << " (" << node.nodes.size()
                      << " elems) : " << node.value << std::endl;
        });

        std::cout << "main() : elapsed (std::chrono overhead)" << std::endl
                  << "  lexer : " << std::chrono::duration_cast<std::chrono::milliseconds>(*const_cast<std::chrono::steady_clock::time_point*>(&lex_end) -
                                                                                          *const_cast<std::chrono::steady_clock::time_point*>(&lex_start)).count() << " ms" << std::endl
        << "  sr(1) : " << std::chrono::duration_cast<std::chrono::milliseconds>(*const_cast<std::chrono::steady_clock::time_point*>(&p_end) -
                                                                                *const_cast<std::chrono::steady_clock::time_point*>(&p_start)).count() << " ms" << std::endl;
    }


    return 0;
}
