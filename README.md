# SuperCFG

A C++ header-only templated shift-reduce parser generator library. Original name stands for context-free-grammar superset generator.

WIP!!

## Features

- Grammar rules definition using templates
- Fully templated parsing pipeline (lexer, parser and a user-defined AST class)
- Shift-reduce parser with optional lookahead(1) algorithm
- Legacy recursive-descent parser (WIP)
- Compile-time rules serialization in custom \*EBNF-like notation

## Getting Started

### Grammar Definition Example

The first step is to define your grammar:

```cpp
#include "cfg/gbnf.h"
#include "cfg/str.h"
#include "cfg/base.h"
#include "cfg/parser.h"
#include "cfg/containers.h"

// ...

// Define non-terminals
constexpr auto digit = NTerm(cs<"digit">());
constexpr auto number = NTerm(cs<"number">());

// Define terminals and grammar rules
// digit := '0'-'9'
constexpr auto d_digit = Define(digit, Alter(
    Term(cs<"1">()), Term(cs<"2">()), Term(cs<"3">()), 
    Term(cs<"4">()), Term(cs<"5">()), Term(cs<"6">()), 
    Term(cs<"7">()), Term(cs<"8">()), Term(cs<"9">()), 
    Term(cs<"0">())
));

// Define a rule for numbers (sequence of digits)
constexpr auto d_number = Define(number, Repeat(digit));

// Combine rules into a ruleset, it's a top-level root definition
constexpr auto ruleset = RulesDef(d_digit, d_number);
```

### Parser Initialization and Usage

Once you have defined your grammar, you can create and use the parser:

```cpp
// ...

// rules definition

// Define string container types for your parser
using VStr = StdStr<char>; // Variable string class inherited from std::string<TChar>
using TokenType = StdStr<char>; // Class used for storing a token type in runtime

// Configure the parser with desired options
constexpr auto conf = mk_sr_parser_conf<
    SRConfEnum::PrettyPrint,  // Enable pretty printing for debugging
    SRConfEnum::Lookahead>(); // Enable lookahead(1)

// Create the shift-reduce parser
// TreeNode<VStr> is the AST class
auto parser = make_sr_parser<VStr, TokenType, TreeNode<VStr>>(ruleset, conf);

// Initialize the tokenizer
Tokenizer<64, VStr, TokenType> lexer(ruleset);
VStr input("12345");
bool ok;

// Generate hashtable for terminals
auto ht = lexer.init_hashtable();

// Tokenize the input
auto tokens = lexer.run(ht, input, ok);

if (ok) {
    // Create a parse tree
    TreeNode<VStr> tree;
    
    // Parse the tokens starting with the 'number' non-terminal
    ok = parser.run(tree, number, tokens);
    
    if (ok) {
        // Process the parse tree
        tree.traverse([&](const auto& node, std::size_t depth) {
            // Print the tree structure
            for (std::size_t i = 0; i < depth; i++) 
                std::cout << "|  ";
            std::cout << node.name << " (" << node.nodes.size() 
                      << " elems) : " << node.value << std::endl;
        });
    }
}
```

## Operators

### Basic Operators

| Name | Description | Example |
|------|-------------|---------|
| Concat | Concatenation | `A,B,C` |
| Alter | Alternation | `A` or `B` or `C` |
| Define | Definition | `A := ...` |
| Optional | None or once | `[A]` |
| Repeat | None or ∞ times | `{A}` |
| Group | Parenthesis | `(A)` |
| Comment | Comment | `(*ABC*)` |
| SpecialSeq | Special sequence | `?ABC?` |
| Except | Exception | `A-B` |
| End | Termination | `ABC.` |
| RulesDef | Rules definition | |

### Extended Operators

| Name | Description |
|------|-------------|
| RepeatExact | Repeat exactly M times |
| RepeatGE | Repeat at least M times |
| RepeatRange | Repeat [M,N] times |

## Grammar serialization

SuperCFG can serialize grammar rules to a custom EBNF-like notation at compile-time. The serialization is done through the `.bake()` method:

```cpp
// Define your grammar rules class
constexpr EBNFBakery rules;

// Define non-terminals and terminals
constexpr auto nozero = NTerm(cs<"digit excluding zero">());
constexpr auto d_nozero = Define(nozero, Alter(
    Term(cs<"1">()), Term(cs<"2">()), Term(cs<"3">()), 
    Term(cs<"4">()), Term(cs<"5">()), Term(cs<"6">()), 
    Term(cs<"7">()), Term(cs<"8">()), Term(cs<"9">())
));

// Reference other non-terminals in definitions
constexpr auto d_digit = Define(NTerm(cs<"digit">()), 
    Alter(Term(cs<"0">()), nozero)
);

// Combine rules and serialize to EBNF-like notation
constexpr auto root = RulesDef(d_nozero, d_digit).bake(rules);

// Now root contains a compile-time string with the serialized grammar:
// "digit excluding zero = \"1\" | \"2\" | \"3\" | \"4\" | \"5\" | \"6\" | \"7\" | \"8\" | \"9\" ;\n"
// "digit = \"0\" | digit excluding zero ;"

// You can output this string at runtime:
std::cout << root.c_str() << std::endl;

// Or even use static_assert to verify the grammar at compile-time:
constexpr char expected[] = "digit excluding zero = \"1\" | \"2\" | \"3\" | \"4\" | \"5\" | \"6\" | \"7\" | \"8\" | \"9\" ;\n"
                           "digit = \"0\" | digit excluding zero ;";
static_assert(cs<expected>() == root, "Grammar has changed!");
```

The serialization also supports automatic operators grouping by analyzing their order.

## Complex Example: Calculator Grammar

Here's a more complex example for a simple calculator grammar:

```cpp
constexpr auto digit = NTerm(cs<"digit">());
constexpr auto d_digit = Define(digit, Alter(Term(cs<"1">()), Term(cs<"2">()), /* ... */));

constexpr auto number = NTerm(cs<"number">());
constexpr auto d_number = Define(number, Repeat(digit));

constexpr auto add = NTerm(cs<"add">());
constexpr auto sub = NTerm(cs<"sub">());
constexpr auto mul = NTerm(cs<"mul">());
constexpr auto div = NTerm(cs<"div">());
constexpr auto op = NTerm(cs<"op">());
constexpr auto arithmetic = NTerm(cs<"arithmetic">());
constexpr auto group = NTerm(cs<"group">());

// No operator order defined: grammar is ambiguous
constexpr auto d_add = Define(add, Concat(op, Term(cs<"+">()), op));
constexpr auto d_sub = Define(sub, Concat(op, Term(cs<"-">()), op));
constexpr auto d_mul = Define(mul, Concat(op, Term(cs<"*">()), op));
constexpr auto d_div = Define(div, Concat(op, Term(cs<"/">()), op));

constexpr auto d_group = Define(group, Concat(Term(cs<"(">()), op, Term(cs<")">())));
constexpr auto d_arithmetic = Define(arithmetic, Alter(add, sub, mul, div));
constexpr auto d_op = Define(op, Alter(number, arithmetic, group));

constexpr auto ruleset = RulesDef(d_digit, d_number, d_add, d_sub, d_mul, d_div, 
                                 d_arithmetic, d_op, d_group);
```
