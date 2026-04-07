
BISON_Y_PROLOGUE = """
// cpp prologue
%{
#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// ast prototype
template<class VStr>
struct TreeNode
{
    VStr name;
    VStr value;
    std::vector<TreeNode<VStr>> nodes;

    TreeNode() = default;
    explicit TreeNode(const VStr& n, const VStr& v = VStr{}) : name(n), value(v) {}
};

using VStr8 = std::string;  // ascii string
using ASTNode = TreeNode<VStr8>;

// helper functions (see supercfg source)
namespace cfg_helpers {

inline std::string hex_decode(const std::string& s)
{
    if (s.size() % 2 != 0)
        throw std::invalid_argument("hex_decode: odd length");

    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        throw std::invalid_argument("hex_decode: invalid character");
    };

    std::string out;
    out.reserve(s.size() / 2);
    for (std::size_t i = 0; i < s.size(); i += 2)
        out += static_cast<char>((nibble(s[i]) << 4) | nibble(s[i + 1]));
    return out;
}

template<class VStr>
VStr hex_encode(const char* data, std::size_t len)
{
    static constexpr char kHex[] = "0123456789abcdef";
    VStr out;
    for (std::size_t i = 0; i < len; ++i) {
        const auto byte = static_cast<std::uint8_t>(data[i]);
        out += kHex[byte >> 4];
        out += kHex[byte & 0x0f];
    }
    return out;
}

template<class VStr>
VStr encode_field(const VStr& s)
{
    VStr out;
    out += VStr(std::to_string(s.size()));
    out += ':';
    out += hex_encode<VStr>(s.data(), s.size());
    return out;
}

} // namespace cfg_helpers

template<class VStr, class TNode>
VStr serialize_ast_wire(const TNode& node)
{
    VStr out;
    out += cfg_helpers::encode_field<VStr>(node.name);  out += '|';
    out += cfg_helpers::encode_field<VStr>(node.value); out += '|';
    out += VStr(std::to_string(node.nodes.size())); out += '|';
    for (const auto& child : node.nodes)
        out += serialize_ast_wire<VStr, TNode>(child);
    return out;
}

// parser state
static VStr8 g_input; // current parse payload
static std::size_t g_pos = 0;
static std::vector<ASTNode> g_pool;  // node arena
static int g_root_idx = -1;

// ast helpers

static inline int new_node(const VStr8& name, const VStr& value = "")
{
    g_pool.emplace_back(name, value);
    return static_cast<int>(g_pool.size()) - 1;
}

static inline void add_child(int parent, int child)
{
    ASTNode snapshot = g_pool[child];
    g_pool[parent].nodes.push_back(std::move(snapshot));
}

// forward decl
int yylex();
void yyerror(const char* msg);
%}

// Declarations header

// Assign index to each token/nonterminal
%union { int node_idx; }
"""

# BISON TOKENS : %token <node_idx> TOK_foo "foo"
# BISON_TYPES : %type  <node_idx> r_expr
# BISON_START : %start _root
# %%
# BISON_GRAMMAR : ...


BISON_Y_EPILOGUE_BEGIN = """
%%

void yyerror(const char* /*msg*/)
{
    /* Errors surface as a non-zero yyparse() return value.
     * Detailed diagnostics can be added here if needed.    */
}

"""

# BISON_LEXER : yylex()

BISON_Y_EPILOGUE_END = """
int main()
{
    for (std::size_t seq = 1; ; ++seq)
    {
        std::string line, tok;
        std::cout << "BISON_READY" << std::endl;

        /* Read one full command line. */
        if (!std::getline(std::cin, line)) {
            std::cout << "BISON_FAIL BAD_INPUT " << seq << std::endl;
            continue;
        }

        std::istringstream ss(line);

        /* First token: the command verb. */
        if (!(ss >> tok)) {
            std::cout << "BISON_FAIL BAD_INPUT " << seq << std::endl;
            continue;
        }

        if (tok == "BISON_EXIT") {
            std::cout << "BISON_OK EXIT " << seq << std::endl;
            return 0;
        }

        if (tok != "BISON_PARSE") {
            std::cout << "BISON_FAIL BAD_CMD " << seq << std::endl;
            continue;
        }

        /* Second token: hex-encoded payload. */
        std::string hex_payload;
        if (!(ss >> hex_payload)) {
            std::cout << "BISON_FAIL BAD_INPUT " << seq << std::endl;
            continue;
        }

        std::string parsed_tok;
        try {
            parsed_tok = cfg_helpers::hex_decode(hex_payload);
        } catch (const std::invalid_argument&) {
            std::cout << "BISON_FAIL BAD_INPUT " << seq << std::endl;
            continue;
        }

        # Reset global parser state
        g_pool.clear();
        g_root_idx = -1;
        g_input = parsed_tok;
        g_pos = 0;

        # Calls yylex() internally
        const bool ok = (yyparse() == 0) && (g_root_idx >= 0);

        if (!ok) {
            std::cout << "BISON_FAIL PARSER_FAIL " << seq << std::endl;
            continue;
        }

        std::cout << "BISON_OK " << serialize_ast_wire<VStr8, ASTNode>(g_pool[g_root_idx]) << " " << seq << std::endl;
    }
}
main();
"""
