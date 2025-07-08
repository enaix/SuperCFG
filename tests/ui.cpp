#include <iostream>
#include <vector>
#include <array>

#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
#define POSIX_TERM_SIZE

#include <unistd.h>
#include <sys/ioctl.h>
#endif

#include "extra/prettyprint.h"

int get_terminal_width() {
    struct winsize w;
    #ifdef POSIX_TERM_SIZE
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) return w.ws_col;
    #endif
    return 80;
}
int get_terminal_height() {
    struct winsize w;
    #ifdef POSIX_TERM_SIZE
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) return w.ws_row;
    #endif
    return 24;
}

int main() {
    // Dummy stack/context for InteractivePrinter
    //vector<int> stack;
    //array<size_t, 1> context = {0};
    InteractivePrinter<int, 1> printer(std::cout);

    int term_w = get_terminal_width();
    int term_h = get_terminal_height();
    printer.init_matrix(term_h, term_w);

    // Root widget (Floating)
    IPWidget root;
    root._layout = IPWidgetLayout::Floating;
    root._box_style = IPBoxStyle::Double;
    root._color = IPColor(IPColor::FG::BrightWhite, IPColor::BG::Blue);
    root._fill_rect_with_color = false;
    root._xy = {0, 0};

    // Horizontal child
    IPWidget horiz;
    horiz._layout = IPWidgetLayout::Horizontal;
    horiz._box_style = IPBoxStyle::Single;
    horiz._color = IPColor(IPColor::FG::BrightYellow, IPColor::BG::Red);
    horiz._fill_rect_with_color = true;
    horiz._xy = {2, 2};
    // Add text children to horizontal
    for (int i = 0; i < 3; ++i) {
        IPWidget t;
        t._layout = IPWidgetLayout::Text;
        t._content = "H-" + std::to_string(i);
        t._color = IPColor(IPColor::FG::BrightGreen, IPColor::BG::Default);
        t._box_style = IPBoxStyle::None;
        horiz._children.push_back(t);
    }

    // Vertical child
    IPWidget vert;
    vert._layout = IPWidgetLayout::Vertical;
    vert._box_style = IPBoxStyle::Single;
    vert._color = IPColor(IPColor::FG::BrightCyan, IPColor::BG::Magenta);
    vert._fill_rect_with_color = true;
    vert._xy = {20, 2};
    for (int i = 0; i < 3; ++i) {
        IPWidget t;
        t._layout = IPWidgetLayout::Text;
        t._content = "V-" + std::to_string(i);
        t._color = IPColor(IPColor::FG::BrightRed, IPColor::BG::Default);
        t._box_style = IPBoxStyle::None;
        vert._children.push_back(t);
    }

    // Floating child
    IPWidget floating;
    floating._layout = IPWidgetLayout::Floating;
    floating._box_style = IPBoxStyle::Single;
    floating._color = IPColor(IPColor::FG::BrightMagenta, IPColor::BG::Green);
    floating._fill_rect_with_color = true;
    floating._xy = {2, 10};
    for (int i = 0; i < 2; ++i) {
        IPWidget t;
        t._layout = IPWidgetLayout::Text;
        t._content = "F-" + std::to_string(i);
        t._color = IPColor(IPColor::FG::BrightBlue, IPColor::BG::Default);
        t._box_style = IPBoxStyle::None;
        t._xy = {i * 8, i * 2};
        floating._children.push_back(t);
    }

    // Text child
    IPWidget text;
    text._layout = IPWidgetLayout::Text;
    text._content = "Just a text widget!";
    text._color = IPColor(IPColor::FG::BrightWhite, IPColor::BG::Red);
    text._box_style = IPBoxStyle::Single;
    text._fill_rect_with_color = false;
    text._xy = {40, 10};

    // Add all to root
    root._children.push_back(horiz);
    root._children.push_back(vert);
    root._children.push_back(floating);
    root._children.push_back(text);

    // Layout and render
    root.layout();
    root.render(printer._output_matrix, printer._color_matrix, 0, 0);
    printer.render_matrix();

    std::cin.ignore();
    return 0;
}
