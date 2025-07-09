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

    explicit IPColor(FG fg = FG::None, BG bg = BG::None) : _fg(fg), _bg(bg) {}

    IPColor(const IPColor& other) : _fg(other._fg), _bg(other._bg) {}

    IPColor(IPColor&& other) noexcept : _fg(std::move(other._fg)), _bg(std::move(other._bg)) {}

    IPColor& operator=(const IPColor& other)
    {
        _fg = other._fg;
        _bg = other._bg;
        return *this;
    }

    IPColor& operator=(IPColor&& other)
    {
        _fg = std::move(other._fg);
        _bg = std::move(other._bg);
        return *this;
    }

    [[nodiscard]] std::string code() const {
        std::ostringstream oss;
        oss << "\033[" << static_cast<int>(_fg) << ";" << static_cast<int>(_bg) << "m";
        return oss.str();
    }
    static std::string reset() { return "\033[0m"; }

    static IPColor None() { return IPColor(FG::None, BG::None); }
    bool isna() const { return *this == IPColor::None(); }

    bool operator==(const IPColor& other) const {
        return _fg == other._fg && _bg == other._bg;
    }
    bool operator!=(const IPColor& other) const {
        return !(*this == other);
    }

    [[nodiscard]] FG fg() const { return _fg; }
    [[nodiscard]] BG bg() const { return _bg; }
    [[nodiscard]] FG& fg() { return _fg; }
    [[nodiscard]] BG& bg() { return _bg; }

    // Mix 2 colors together
    IPColor blend(const IPColor& with) const {
        IPColor res = *this;
        if (res.fg() == IPColor::FG::None) res.fg() = with.fg();
        if (res.bg() == IPColor::BG::None) res.bg() = with.bg();
        return res;
    }

    // Overlay this color with another
    IPColor overlay(const IPColor& with) const {
        IPColor res = *this;
        if (with.fg() != IPColor::FG::None) res.fg() = with.fg();
        if (with.bg() != IPColor::BG::None) res.bg() = with.bg();
        return res;
    }

private:
    FG _fg;
    BG _bg;
};


enum class IPColors : int
{
    Primary,
    Secondary,
    Accent,
    Selected,
    Inactive, // Not selected
    Disabled, // Inactive window
    BorderActive,
    BorderInactive,
    BorderDisabled, // Inactive window
    None // last elem
};


// IPColorPalette: Template for color palettes (e.g., ANSI, TrueColor)
template <class TColor>
class IPAppStyle {
protected:
    std::array<TColor, static_cast<size_t>(IPColors::None)> _colors;
    int _color_overload;

public:
    template<class... TColors>
    IPAppStyle(TColors... c) : _colors{c...}, _color_overload(-1) {}

    TColor get_color(IPColors color) const
    {
        if (_color_overload != -1)
            return _colors[_color_overload];

        for (int i = static_cast<int>(color); i >= 0; i--)
            if (!_colors[i].isna())
                return _colors[i];
        return TColor::None();
    }
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


// Event types for user interaction
enum class IPEventType {
    None,
    Select,
    Click, // KeyDown
    ArrowUp,
    ArrowDown,
    ArrowLeft,
    ArrowRight,
    OnCreate,
    OnDestroy
};

struct IPEvent {
    IPEventType type;
    int key; // For Click, can be ASCII or special key code
    IPEvent(IPEventType t = IPEventType::None, int k = 0) : type(t), key(k) {}
};


class IPWidget;
class IPWindow;
// Event handler signature for all widgets
using IPEventHandler = void(*)(IPWidget*, IPWindow*, const IPEvent&);


class IPWidget
{
public:
    IPPoint _xy; // Relative to the parent
    IPPoint _wh; // width, height
    IPColors _color;
    IPQuad _margin; // left, top, right, bottom
    IPQuad _padding; // left, top, right, bottom
    std::vector<IPWidget> _children;
    std::string _content; // Text content
    IPShadowStyle _shadow_style;

    IPWidgetLayout _layout;

    IPBoxStyle _box_style;

    bool selectable = false;
    bool selected = false;
    IPEventHandler on_event = nullptr;

    // Helper to get effective color from palette if set
    template<class TStyle>
    IPColor get_effective_color(const TStyle& style, bool active_window) const {
        if (!active_window)
            return style.get_color(IPColors::Disabled);
        if (selectable)
        {
            if (selected)
                return style.get_color(IPColors::Selected);
            return style.get_color(IPColors::Inactive);
        }
        return style.get_color(_color);
    }

    // Helper to get effective border color from palette if set
    template<class TStyle>
    IPColor get_border_color(const TStyle& style, bool active_window) const {
        if (!active_window)
            return style.get_color(IPColors::BorderDisabled);

        if (selectable && selected)
            return style.get_color(IPColors::BorderActive);
    
        return style.get_color(IPColors::BorderInactive);
    }

    // Find next selectable child (for arrow navigation)
    int find_next_selectable(int start, int dir) const {
        int n = (int)_children.size();
        for (int i = 1; i <= n; ++i) {
            int idx = (start + dir * i + n) % n;
            if (_children[idx].selectable) return idx;
        }
        return -1;
    }

    // Deselect all children
    void deselect_all() {
        for (auto& child : _children) child.selected = false;
    }

    IPWidget()
        : _xy{0, 0}, _wh{0, 0}, _color(IPColors::Primary),
          _margin{0, 0, 0, 0}, _padding{0, 0, 0, 0}, _shadow_style(IPShadowStyle::None)
        , _layout(IPWidgetLayout::Text), _box_style(IPBoxStyle::None)
    {}

    // Text box
    explicit IPWidget(std::string text, IPColors color = IPColors::Primary, IPQuad  margin = IPQuad(0, 0, 0, 0), const IPBoxStyle box = IPBoxStyle::None, const IPShadowStyle shadow = IPShadowStyle::None)
        : _content(std::move(text)), _color(color), _margin(std::move(margin)), _box_style(box), _shadow_style(shadow), _layout(IPWidgetLayout::Text)
    {}

    // Horizontal/Vertical layout
    IPWidget(IPWidgetLayout layout, std::vector<IPWidget> children, IPColors color = IPColors::Primary, IPQuad margin = IPQuad(0,0,0,0), IPQuad padding = IPQuad(0,0,0,0), IPBoxStyle box = IPBoxStyle::None, IPShadowStyle shadow = IPShadowStyle::None, IPPoint xy = {0,0})
        : _xy(xy), _wh{0,0}, _color(color), _margin(margin), _padding(padding), _children(std::move(children)), _content(), _shadow_style(shadow), _layout(layout), _box_style(box)
    {}

    // Floating layout
    IPWidget(const IPPoint& xy, std::vector<IPWidget> children, IPColors color = IPColors::Primary, IPQuad margin = IPQuad(0,0,0,0), IPQuad padding = IPQuad(0,0,0,0), IPBoxStyle box = IPBoxStyle::None, IPShadowStyle shadow = IPShadowStyle::None)
        : _xy(xy), _wh{0, 0}, _color(color), _margin(margin), _padding(padding), _children(std::move(children)), _content(), _shadow_style(shadow), _layout(IPWidgetLayout::Floating), _box_style(box)
    {}

    // Full constructor
    IPWidget(const IPPoint& xy, const IPPoint& wh, IPColors color, IPQuad margin, IPQuad padding, std::vector<IPWidget> children, std::string content, IPShadowStyle shadow, IPWidgetLayout layout, IPBoxStyle box)
        : _xy(xy), _wh(wh), _color(color), _margin(margin), _padding(padding), _children(std::move(children)), _content(std::move(content)), _shadow_style(shadow), _layout(layout), _box_style(box)
    {}

    // Layout/rendering logic
    // ======================

    template<class TColor>
    static void draw_box(std::vector<std::string>& matrix, std::vector<std::vector<TColor>>& color_matrix, int x, int y, int w, int h, IPBoxStyle style, const TColor& color) {
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
                color_matrix[row][col] = color_matrix[row][col].overlay(color);
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

    void layout() {
        auto [ml, mt, mr, mb] = _margin.tup();
        auto [pl, pt, pr, pb] = _padding.tup();

        switch (_layout) {
            case IPWidgetLayout::Horizontal: {
                int x = 0, max_h = 0;
                
                for (int i = 0; i < _children.size(); i++) {
                    _children[i].layout();
                    
                    if (i > 0)
                        x += pl;
                    if (i < _children.size() - 1)
                        x += pr;

                    x += _children[i]._wh.w();
                    max_h = std::max(max_h, _children[i]._wh.h());
                }
                _wh.w() = x + ml + mr;
                _wh.h() = max_h + mt + mb;
                break;
            }
            case IPWidgetLayout::Vertical: {
                int y = 0, max_w = 0;
                for (int i = 0; i < _children.size(); i++) {
                    _children[i].layout();

                    if (i > 0)
                        y += pl;
                    if (i < _children.size() - 1)
                        y += pr;
                    
                    y += _children[i]._wh.h();
                    max_w = std::max(max_w, _children[i]._wh.w());
                }
                _wh.w() = max_w + ml + mr;
                _wh.h() = y + mt + mb;
                break;
            }
            case IPWidgetLayout::Floating: {
                int max_x = 0, max_y = 0;
                for (auto& child : _children) {
                    child.layout();
                    int child_x = child._xy.x() + child._wh.w();
                    int child_y = child._xy.y() + child._wh.h();
                    max_x = std::max(max_x, child_x);
                    max_y = std::max(max_y, child_y);
                }
                _wh.w() = max_x + ml + mr;
                _wh.h() = max_y + mt + mb;
                break;
            }
            case IPWidgetLayout::Text: {
                _wh.w() = static_cast<int>(_content.size()) + ml + mr;
                _wh.h() = 1 + mt + mb;
                break;
            }
        }

        // If box is present, increment size for box border
        if (_box_style != IPBoxStyle::None) {
            //_margin += 1;
            _wh += 2;
        }
    }

    template<class TColor, template<class> class TStyle>
    void render(std::vector<std::string>& matrix, std::vector<std::vector<TColor>>& color_matrix, const TStyle<TColor>& style, bool active_window, int x, int y, const TColor& parent_color = TColor::None()) {
        IPColor effective_color = get_effective_color(style, active_window).blend(parent_color);
        auto [ml, mt, mr, mb] = _margin.tup();
        auto [pl, pt, pr, pb] = _padding.tup();
        // Draw box if needed
        if (_box_style != IPBoxStyle::None) {
            IPColor border_color = get_border_color(style, active_window).blend(parent_color);
            draw_box(matrix, color_matrix, x, y, _wh.w(), _wh.h(), _box_style, border_color);
            x += 1; y += 1;
        }

        switch (_layout) {
            case IPWidgetLayout::Horizontal: {
                int cur_x = x + ml;
                for (int i = 0; i < _children.size(); i++) {
                    if (i > 0)
                        cur_x += pl;
                    _children[i].render(matrix, color_matrix, style, active_window, cur_x, y + mt, effective_color);
                    if (i < _children.size() - 1)
                        cur_x += pr;
                    cur_x += _children[i]._wh.w();
                }
                break;
            }
            case IPWidgetLayout::Vertical: {
                int cur_y = y + mt;
                for (int i = 0; i < _children.size(); i++) {
                    if (i > 0)
                        cur_y += pt;
                    _children[i].render(matrix, color_matrix, style, active_window, x + ml, cur_y, effective_color);
                    if (i < _children.size() - 1)
                        cur_y += pb;
                    cur_y += _children[i]._wh.h();
                }
                break;
            }
            case IPWidgetLayout::Floating: {
                for (auto& child : _children) {
                    child.render(matrix, color_matrix, style, active_window, x + child._xy.x() + ml, y + child._xy.y() + mt, effective_color);
                }
                break;
            }
            case IPWidgetLayout::Text: {
                int cur_x = x + ml, cur_y = y + mt;
                if (cur_y < static_cast<int>(matrix.size()) && cur_x + static_cast<int>(_content.size()) <= static_cast<int>(matrix[0].size())) {
                    for (size_t i = 0; i < _content.size(); ++i) {
                        matrix[cur_y][cur_x + i] = _content[i];
                        color_matrix[cur_y][cur_x + i] = color_matrix[cur_y][cur_x + i].overlay(effective_color);
                    }
                }
                break;
            }
        }

        // Fill the rectangle with color if the flag is set
        int box_offset = (_box_style == IPBoxStyle::None ? 0 : 2);
        int shadow_size = (_box_style == IPBoxStyle::None ? 1 : 2);

        switch (_shadow_style)
        {
            case IPShadowStyle::Fill:
                for (int row = 0; row < _wh.h() - box_offset; ++row) {
                    int abs_y = y + row;
                    if (abs_y < 0 || abs_y >= static_cast<int>(color_matrix.size())) continue;
                    for (int col = 0; col < _wh.w() - box_offset; ++col) {
                        int abs_x = x + col;
                        if (abs_x < 0 || abs_x >= static_cast<int>(color_matrix[0].size())) continue;
                        color_matrix[abs_y][abs_x] = color_matrix[abs_y][abs_x].blend(effective_color);
                    }
                }
                break;
            case IPShadowStyle::Shadow:
                for (int row = 0; row < _wh.h() - box_offset + shadow_size; ++row) {
                    int abs_y = y + row;
                    if (abs_y < 0 || abs_y >= static_cast<int>(color_matrix.size())) continue;
                    for (int col = 0; col < _wh.w() - box_offset + shadow_size; ++col) {
                        int abs_x = x + col;
                        if (abs_x < 0 || abs_x >= static_cast<int>(color_matrix[0].size())) continue;
                        color_matrix[abs_y][abs_x] = color_matrix[abs_y][abs_x].blend(effective_color);
                    }
                }
                break;
            default:
                break;
        }
    }
};


// Window stack and selector logic
class IPWindow {
public:
    std::vector<IPWidget> stack; // Topmost is last
    int selector_idx = -1; // Index of selected widget in stack

    IPWindow() = default;

    void push(IPWidget w) {
        stack.push_back(std::move(w));
        selector_idx = (int)stack.size() - 1;
        if (selector_idx >= 0) {
            stack[selector_idx].selected = true;
            if (stack[selector_idx].on_event)
                stack[selector_idx].on_event(&stack[selector_idx], this, IPEvent(IPEventType::OnCreate));
        }
    }
    void pop() {
        if (!stack.empty()) {
            if (stack.back().on_event)
                stack.back().on_event(&stack.back(), this, IPEvent(IPEventType::OnDestroy));
            stack.pop_back();
            selector_idx = (int)stack.size() - 1;
            if (selector_idx >= 0)
                stack[selector_idx].selected = true;
        }
    }
    IPWidget* top() { return stack.empty() ? nullptr : &stack.back(); }

    // Move selector to next selectable window
    void move_selector(int dir) {
        if (stack.empty()) return;
        int n = (int)stack.size();
        int start = selector_idx;
        for (int i = 1; i <= n; ++i) {
            int idx = (start + dir * i + n) % n;
            if (stack[idx].selectable) {
                if (selector_idx >= 0) stack[selector_idx].selected = false;
                selector_idx = idx;
                stack[selector_idx].selected = true;
                return;
            }
        }
    }

    // Move selection within the top window's children
    void move_child_selector(int dir) {
        if (selector_idx < 0 || selector_idx >= (int)stack.size()) return;
        IPWidget& win = stack[selector_idx];
        int n = (int)win._children.size();
        int cur = 0;
        for (int i = 0; i < n; ++i) if (win._children[i].selected) cur = i;
        int next = win.find_next_selectable(cur, dir);
        win.deselect_all();
        if (next >= 0) win._children[next].selected = true;
    }

    // Route event to selected window and its selected child
    void handle_event(const IPEvent& ev) {
        if (selector_idx >= 0 && selector_idx < (int)stack.size()) {
            IPWidget* win = &stack[selector_idx];
            if (ev.type == IPEventType::ArrowUp || ev.type == IPEventType::ArrowDown) {
                move_child_selector(ev.type == IPEventType::ArrowDown ? 1 : -1);
            } else if (ev.type == IPEventType::Select || ev.type == IPEventType::Click) {
                for (auto& child : win->_children) {
                    if (child.selected && child.on_event)
                        child.on_event(&child, this, ev);
                }
            } else if (win->on_event) {
                win->on_event(win, this, ev);
            }
        }
    }
};


template<class TColor>
class InteractivePrinter
{
public:
    std::ostream& _os;

    // Pointers to the data
    //const std::vector<GSymbol>& _stack; // Symbols stack
    //const std::array<std::size_t, ContextSize>& _context; //

    // Output matrix for rendering: each string is a row
    std::vector<std::string> _output_matrix;
    std::vector<std::vector<TColor>> _color_matrix;
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
        _color_matrix.assign(rows, std::vector<TColor>(cols, IPColor::None()));
    }

    void set_cell(std::size_t row, std::size_t col, char value, const TColor& color = TColor::None()) {
        if (row < _rows && col < _cols) {
            _output_matrix[row][col] = value;
            _color_matrix[row][col] = color;
        }
    }

    void set_text(std::size_t row, std::size_t col, const std::string& text, const TColor& color = TColor::None()) {
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
