//
// Created by Flynn on 08.07.2025.
//

#ifndef PRETTYPRINT_H
#define PRETTYPRINT_H

#include <iostream>
#include <utility>
#include <vector>
#include <array>
#include <string>
#include <sstream>
#include <tuple>


// IPColor class for handling ANSI color codes
class IPColor {
public:
    enum class FG : int {
        Default = 39,
        Black = 30,
        Red = 31,
        Green = 32,
        Yellow = 33,
        Blue = 34,
        Magenta = 35,
        Cyan = 36,
        White = 37,
        BrightBlack = 90,
        BrightRed = 91,
        BrightGreen = 92,
        BrightYellow = 93,
        BrightBlue = 94,
        BrightMagenta = 95,
        BrightCyan = 96,
        BrightWhite = 97,
        None = 0
    };
    enum class BG : int {
        Default = 49,
        Black = 40,
        Red = 41,
        Green = 42,
        Yellow = 43,
        Blue = 44,
        Magenta = 45,
        Cyan = 46,
        White = 47,
        BrightBlack = 100,
        BrightRed = 101,
        BrightGreen = 102,
        BrightYellow = 103,
        BrightBlue = 104,
        BrightMagenta = 105,
        BrightCyan = 106,
        BrightWhite = 107,
        None = 0
    };

    explicit IPColor(FG fg = FG::Default, BG bg = BG::Default) : _fg(fg), _bg(bg) {}

    [[nodiscard]] std::string code() const {
        std::ostringstream oss;
        oss << "\033[" << static_cast<int>(_fg) << ";" << static_cast<int>(_bg) << "m";
        return oss.str();
    }
    static std::string reset() { return "\033[0m"; }

    static IPColor None() { return IPColor(FG::None, BG::None); }

    bool operator==(const IPColor& other) const {
        return _fg == other._fg && _bg == other._bg;
    }
    bool operator!=(const IPColor& other) const {
        return !(*this == other);
    }

    [[nodiscard]] FG fg() const { return _fg; }
    [[nodiscard]] BG bg() const { return _bg; }
    FG& fg() { return _fg; }
    BG& bg() { return _bg; }

private:
    FG _fg;
    BG _bg;
};


enum class IPWidgetLayout {
    Horizontal, // Use margin and padding to position children
    Vertical, // ditto
    Floating, // Use widgets _xy to position
    Text // Just text (std::string), no children
};

enum class IPBoxStyle {
    None,
    Single,
    Double
};

enum class IPShadowStyle {
    None,
    Fill, // No shadow, just background color
    Shadow // Background color and a shadow
};


class IPQuad : public std::tuple<int, int, int, int> // left, top, right, bottom
{
public:
    using std::tuple<int, int, int, int>::tuple; // using constructors

    // 1d swizzling
    int& l() { return std::get<0>(*this); }
    int& t() { return std::get<1>(*this); }
    int& r() { return std::get<2>(*this); }
    int& b() { return std::get<3>(*this); }

    int l() const { return std::get<0>(*this); }
    int t() const { return std::get<1>(*this); }
    int r() const { return std::get<2>(*this); }
    int b() const { return std::get<3>(*this); }

    // Access quad as a native tuple object. No overhead
    std::tuple<int, int, int, int>& tup() { return static_cast<std::tuple<int, int, int, int>&>(*this); }

    // Operators overload
    IPQuad& operator+=(const IPQuad& rhs)
    { l() += rhs.l();  t() += rhs.t();  r() += rhs.r();  b() += rhs.b();  return *this; }
    IPQuad& operator-=(const IPQuad& rhs)
    { l() -= rhs.l();  t() -= rhs.t();  r() -= rhs.r();  b() -= rhs.b();  return *this; }
    IPQuad& operator*=(const IPQuad& rhs)
    { l() *= rhs.l();  t() *= rhs.t();  r() *= rhs.r();  b() *= rhs.b();  return *this; }
    IPQuad& operator/=(const IPQuad& rhs)
    { l() /= rhs.l();  t() /= rhs.t();  r() /= rhs.r();  b() /= rhs.b();  return *this; }

    IPQuad& operator+=(int a)
    { l() += a;  t() += a;  r() += a;  b() += a;  return *this; }
    IPQuad& operator-=(int a)
    { l() -= a;  t() -= a;  r() -= a;  b() -= a;  return *this; }
    IPQuad& operator*=(int a)
    { l() *= a;  t() *= a;  r() *= a;  b() *= a;  return *this; }
    IPQuad& operator/=(int a)
    { l() /= a;  t() /= a;  r() /= a;  b() /= a;  return *this; }
};


class IPPoint : public std::pair<int, int>
{
public:
    using std::pair<int, int>::pair; // using constructors

    // 1d swizzling
    int& x() { return this->first; }
    int& y() { return this->second; }
    int& w() { return this->first; }  // width
    int& h() { return this->second; } // height

    int x() const { return this->first; }
    int y() const { return this->second; }
    int w() const { return this->first; }
    int h() const { return this->second; }

    // Access point as a native pair object. No overhead
    std::pair<int, int>& pair() { return static_cast<std::pair<int, int>&>(*this); }

    // Operators overload
    IPPoint& operator+=(const IPPoint& rhs)
    { x() += rhs.x();  y() += rhs.y();  return *this; }
    IPPoint& operator-=(const IPPoint& rhs)
    { x() -= rhs.x();  y() -= rhs.y();  return *this; }
    IPPoint& operator*=(const IPPoint& rhs)
    { x() *= rhs.x();  y() *= rhs.y();  return *this; }
    IPPoint& operator/=(const IPPoint& rhs)
    { x() /= rhs.x();  y() /= rhs.y();  return *this; }

    IPPoint& operator+=(int a)
    { x() += a;  y() += a;  return *this; }
    IPPoint& operator-=(int a)
    { x() -= a;  y() -= a;  return *this; }
    IPPoint& operator*=(int a)
    { x() *= a;  y() *= a;  return *this; }
    IPPoint& operator/=(int a)
    { x() /= a;  y() /= a;  return *this; }
};


class IPWidget
{
public:
    IPPoint _xy; // Relative to the parent
    IPPoint _wh; // width, height
    IPColor _color;
    IPColor _color_override;
    IPQuad _margin; // left, top, right, bottom
    IPQuad _padding; // left, top, right, bottom
    std::vector<IPWidget> _children;
    std::string _content; // Text content
    IPShadowStyle _shadow_style;

    IPWidgetLayout _layout;

    IPBoxStyle _box_style;

    IPWidget()
        : _xy{0, 0}, _wh{0, 0}, _color(IPColor::None()), _color_override(IPColor::None()),
          _margin{0, 0, 0, 0}, _padding{0, 0, 0, 0}, _shadow_style(IPShadowStyle::None)
        , _layout(IPWidgetLayout::Text), _box_style(IPBoxStyle::None)
    {}

    // Text box
    explicit IPWidget(std::string text, IPColor color, IPQuad  margin = IPQuad(0, 0, 0, 0), const IPBoxStyle box = IPBoxStyle::None, const IPShadowStyle shadow = IPShadowStyle::None)
        : _content(std::move(text)), _color(color), _margin(std::move(margin)), _box_style(box), _shadow_style(shadow), _layout(IPWidgetLayout::Text)
    {}

    // Horizontal/Vertical layout
    IPWidget(IPWidgetLayout layout, std::vector<IPWidget> children, IPColor color = IPColor::None(), IPQuad margin = IPQuad(0,0,0,0), IPQuad padding = IPQuad(0,0,0,0), IPBoxStyle box = IPBoxStyle::None, IPShadowStyle shadow = IPShadowStyle::None, IPPoint xy = {0,0})
        : _xy(xy), _wh{0,0}, _color(color), _color_override(IPColor::None()), _margin(margin), _padding(padding), _children(std::move(children)), _content(), _shadow_style(shadow), _layout(layout), _box_style(box)
    {}

    // Floating layout
    IPWidget(const IPPoint& xy, std::vector<IPWidget> children, IPColor color = IPColor::None(), IPQuad margin = IPQuad(0,0,0,0), IPQuad padding = IPQuad(0,0,0,0), IPBoxStyle box = IPBoxStyle::None, IPShadowStyle shadow = IPShadowStyle::None)
        : _xy(xy), _wh{0, 0}, _color(color), _color_override(IPColor::None()), _margin(margin), _padding(padding), _children(std::move(children)), _content(), _shadow_style(shadow), _layout(IPWidgetLayout::Floating), _box_style(box)
    {}

    // Full constructor
    IPWidget(const IPPoint& xy, const IPPoint& wh, IPColor color, IPColor color_override, IPQuad margin, IPQuad padding, std::vector<IPWidget> children, std::string content, IPShadowStyle shadow, IPWidgetLayout layout, IPBoxStyle box)
        : _xy(xy), _wh(wh), _color(color), _color_override(color_override), _margin(margin), _padding(padding), _children(std::move(children)), _content(std::move(content)), _shadow_style(shadow), _layout(layout), _box_style(box)
    {}

    // Layout/rendering logic
    // ======================

    static IPColor inherit_color(const IPColor& parent, const IPColor& self, const IPColor& override) {
        IPColor result = self;
        if (override.fg() != IPColor::FG::None) result.fg() = override.fg();
        if (override.bg() != IPColor::BG::None) result.bg() = override.bg();
        if (result.fg() == IPColor::FG::None) result.fg() = parent.fg();
        if (result.bg() == IPColor::BG::None) result.bg() = parent.bg();
        return result;
    }

    static void draw_box(std::vector<std::string>& matrix, std::vector<std::vector<IPColor>>& color_matrix, int x, int y, int w, int h, IPBoxStyle style, const IPColor& color) {
        char tl, tr, bl, br, hline, vline;

        switch (style)
        {
        case IPBoxStyle::Single:
            tl = '+'; tr = '+'; bl = '+'; br = '+'; hline = '-'; vline = '|';
            break;
        case IPBoxStyle::Double:
            tl = '#'; tr = '#'; bl = '#'; br = '#'; hline = '='; vline = 'H';
            break;
        default:
            return; // None
        }

        if (w < 2 || h < 2) return;
        int x2 = x + w - 1, y2 = y + h - 1;

        auto set = [&](int row, int col, char ch) {
            if (row >= 0 && row < (int)matrix.size() && col >= 0 && col < (int)matrix[0].size()) {
                matrix[row][col] = ch;
                color_matrix[row][col] = color;
            }
        };

        // Corners
        set(y, x, tl);
        set(y, x2, tr);
        set(y2, x, bl);
        set(y2, x2, br);
        // Top and bottom edges
        for (int i = x + 1; i < x2; ++i) {
            set(y, i, hline);
            set(y2, i, hline);
        }
        // Left and right edges
        for (int i = y + 1; i < y2; ++i) {
            set(i, x, vline);
            set(i, x2, vline);
        }
    }

    void layout(const IPColor& parent_color = IPColor::None()) {
        IPColor effective_color = inherit_color(parent_color, _color, _color_override);

        auto [ml, mt, mr, mb] = _margin.tup();
        auto [pl, pt, pr, pb] = _padding.tup();

        switch (_layout) {
            case IPWidgetLayout::Horizontal: {
                int x = 0, max_h = 0;
                for (auto& child : _children) {
                    child.layout(effective_color);
                    auto [cml, cmt, cmr, cmb] = child._margin.tup();
                    x += cml;
                    x += child._wh.w() + cmr;
                    max_h = std::max(max_h, child._wh.h() + cmt + cmb);
                }
                _wh.w() = x + pl + pr;
                _wh.h() = max_h + pt + pb;
                break;
            }
            case IPWidgetLayout::Vertical: {
                int y = 0, max_w = 0;
                for (auto& child : _children) {
                    child.layout(effective_color);
                    auto [cml, cmt, cmr, cmb] = child._margin.tup();
                    y += cmt;
                    y += child._wh.h() + cmb;
                    max_w = std::max(max_w, child._wh.w() + cml + cmr);
                }
                _wh.w() = max_w + pl + pr;
                _wh.h() = y + pt + pb;
                break;
            }
            case IPWidgetLayout::Floating: {
                int max_x = 0, max_y = 0;
                for (auto& child : _children) {
                    child.layout(effective_color);
                    int child_x = child._xy.x() + child._wh.w();
                    int child_y = child._xy.y() + child._wh.h();
                    max_x = std::max(max_x, child_x);
                    max_y = std::max(max_y, child_y);
                }
                _wh.w() = max_x;
                _wh.h() = max_y;
                break;
            }
            case IPWidgetLayout::Text: {
                _wh.w() = static_cast<int>(_content.size()) + pl + pr;
                _wh.h() = 1 + pt + pb;
                break;
            }
        }

        // If box is present, increment size for box border
        if (_box_style != IPBoxStyle::None) {
            //_margin += 1;
            _wh += 2;
        }
    }

    void render(std::vector<std::string>& matrix, std::vector<std::vector<IPColor>>& color_matrix, int x, int y, const IPColor& parent_color = IPColor::None()) {
        IPColor effective_color = inherit_color(parent_color, _color, _color_override);
        auto [pl, pt, pr, pb] = _padding.tup();
        // Draw box if needed
        if (_box_style != IPBoxStyle::None) {
            draw_box(matrix, color_matrix, x, y, _wh.w(), _wh.h(), _box_style, effective_color);
            x += 1; y += 1;
        }
        // Fill the rectangle with color if the flag is set
        switch (_shadow_style)
        {
            case IPShadowStyle::Fill:
                for (int row = 0; row < _wh.h(); ++row) {
                    int abs_y = y + row;
                    if (abs_y < 0 || abs_y >= static_cast<int>(color_matrix.size())) continue;
                    for (int col = 0; col < _wh.w(); ++col) {
                        int abs_x = x + col;
                        if (abs_x < 0 || abs_x >= static_cast<int>(color_matrix[0].size())) continue;
                        color_matrix[abs_y][abs_x] = effective_color;
                    }
                }
                break;
            case IPShadowStyle::Shadow:
                for (int row = 0; row < _wh.h(); ++row) {
                    int abs_y = y + row - 1;
                    if (abs_y < 0 || abs_y >= static_cast<int>(color_matrix.size())) continue;
                    for (int col = 0; col < _wh.w(); ++col) {
                        int abs_x = x + col - 1;
                        if (abs_x < 0 || abs_x >= static_cast<int>(color_matrix[0].size())) continue;
                        color_matrix[abs_y][abs_x] = effective_color;
                    }
                }
                break;
            default:
                break;
        }

        switch (_layout) {
            case IPWidgetLayout::Horizontal: {
                int cur_x = x + pl;
                for (auto& child : _children) {
                    child.render(matrix, color_matrix, cur_x, y + pt, effective_color);
                    cur_x += child._wh.w();
                }
                break;
            }
            case IPWidgetLayout::Vertical: {
                int cur_y = y + pt;
                for (auto& child : _children) {
                    child.render(matrix, color_matrix, x + pl, cur_y, effective_color);
                    cur_y += child._wh.h();
                }
                break;
            }
            case IPWidgetLayout::Floating: {
                for (auto& child : _children) {
                    child.render(matrix, color_matrix, x + child._xy.x(), y + child._xy.y(), effective_color);
                }
                break;
            }
            case IPWidgetLayout::Text: {
                if (y < static_cast<int>(matrix.size()) && x + static_cast<int>(_content.size()) <= static_cast<int>(matrix[0].size())) {
                    for (size_t i = 0; i < _content.size(); ++i) {
                        matrix[y][x + i] = _content[i];
                        color_matrix[y][x + i] = effective_color;
                    }
                }
                break;
            }
        }
    }
};


template<class GSymbol, std::size_t ContextSize>
class InteractivePrinter
{
public:
    std::ostream& _os;

    // Pointers to the data
    //const std::vector<GSymbol>& _stack; // Symbols stack
    //const std::array<std::size_t, ContextSize>& _context; //

    // Output matrix for rendering: each string is a row
    std::vector<std::string> _output_matrix;
    std::vector<std::vector<IPColor>> _color_matrix;
    std::size_t _rows = 0;
    std::size_t _cols = 0;


    explicit InteractivePrinter(std::ostream& os) : _os(os) {}

    /*void set_data(const std::vector<GSymbol>& stack, const std::array<std::size_t, ContextSize>& context) {
        _stack = stack;
        _context = context;
    }*/

    void init_matrix(std::size_t rows, std::size_t cols) {
        _rows = rows;
        _cols = cols;
        _output_matrix.assign(rows, std::string(cols, ' '));
        _color_matrix.assign(rows, std::vector<IPColor>(cols, IPColor()));
    }

    void set_cell(std::size_t row, std::size_t col, char value, const IPColor& color = IPColor()) {
        if (row < _rows && col < _cols) {
            _output_matrix[row][col] = value;
            _color_matrix[row][col] = color;
        }
    }
    void set_text(std::size_t row, std::size_t col, const std::string& text, const IPColor& color = IPColor()) {
        if (row < _rows && col + text.size() <= _cols) {
            for (size_t i = 0; i < text.size(); ++i) {
                _output_matrix[row][col + i] = text[i];
                _color_matrix[row][col + i] = color;
            }
        }
    }

    void render_matrix() {
        for (std::size_t r = 0; r < _rows; ++r) {
            for (std::size_t c = 0; c < _cols; ++c) {
                _os << _color_matrix[r][c].code() << _output_matrix[r][c] << IPColor::reset();
            }
            _os << '\n';
        }
    }

    void get_terminal_size()
    {
        // Fetch current terminal window size
    }

    void render_grid()
    {
        // Find the starting and ending positions, render the stack in the middle of the screen
    }

    void print()
    {
        // Render the output
        render_matrix();
    }
};


#endif //PRETTYPRINT_H
