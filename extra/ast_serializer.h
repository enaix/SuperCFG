#ifndef AST_SERIALIZER
#define AST_SERIALIZER

#include <string>
#include <sstream>
#include <iomanip>
#include <cstdint>

// ============================================================================
//  AST Wire Format
// ============================================================================
//  Every node is serialized as <name_field> '|' <value_field> '|' <child_count> '|' <child_0> <child_1> ...
//  Each string field is length-prefixed + hex-body: <field> ::= <decimal_byte_len> ':' <hex_chars>
//  Extremely inefficient


namespace cfg_helpers {

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

/**
 * @brief Parse hex-encoded char to hex value
 */
inline std::uint8_t hex_nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    throw std::invalid_argument(std::string("hex_nibble() : invalid hex character '") + c + '\'');
}

template<class VStr>
VStr hex_decode(const VStr& data)
{
    if (data.size() % 2 != 0)
        throw std::invalid_argument(std::string("hex_decode() : bad input size"));
    VStr out;
    out.reserve(data.size() / 2);
    for (std::size_t i = 0; i < data.size(); i += 2)
        out += static_cast<char>((hex_nibble(data[i]) << 4) | hex_nibble(data[i + 1]));
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

/**
 * @brief Serialize TreeNode<VStr> into a wire format. Supports only char_t for now
 */
template<class VStr, class TTreeNode>
VStr serialize_ast_wire(const TTreeNode& node)
{
    VStr out;

    out += cfg_helpers::encode_field<VStr>(node.name);
    out += '|';

    out += cfg_helpers::encode_field<VStr>(node.value);
    out += '|';

    out += VStr(std::to_string(node.nodes.size()));
    out += '|';

    for (const auto& child : node.nodes)
        out += serialize_ast_wire<VStr, TTreeNode>(child);

    return out;
}


#endif // AST_SERIALIZER
