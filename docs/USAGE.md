# Usage

## Parser and lexer initialization

Parser/lexer configuration flags are described [in `CONFIGURATION.md`](CONFIGURATION.md)

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
    SRConfEnum::Lookahead,    // Enable lookahead(1)
    SRConfEnum::ReducibilityChecker>(); // Enable RC(1), which checks for reducibility for one step ahead

// Initialize the lexer
// There are two lexer types available:

// 1. Legacy Lexer - simpler but with limitations
// Use this for simple grammars where tokens don't appear in multiple rules
auto legacy_lexer = make_lexer<VStr, TokenType>(ruleset, mk_lexer_conf<LexerConfEnum::Legacy>());

// 2. Advanced Lexer - more powerful but slightly slower
// Use this for complex grammars where the same token may appear in different rules
// (e.g., in JSON grammar where ',' appears in both object members and other contexts)

// HandleDuplicates flag enables terms range support and tokens which are present in >2 rules at once. Imposes significant compile-time overhead on grammars with high number of terminals
// HandleDupInRuntime flag moves symbols intersections handling to the lexer initialization in runtime

auto advanced_lexer = make_lexer<VStr, TokenType>(ruleset, mk_lexer_conf<LexerConfEnum::AdvancedLexer, LexerConfEnum::HandleDuplicates>());


// Create the shift-reduce parser
// TreeNode<VStr> is the AST class
auto parser = make_sr_parser<VStr, TokenType, TreeNode<VStr>>(ruleset, advanced_lexer, conf);

VStr input("12345");
bool ok;

// Tokenize the input
auto tokens = advanced_lexer.run(input, ok);

if (ok) {
    // Create a parse tree
    TreeNode<VStr> tree;
    
    // Parse the tokens with 'number' being the root
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

## Grammar serialization

The serialization is done through the `.bake()` method:

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
