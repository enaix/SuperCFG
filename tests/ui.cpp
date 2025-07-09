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
    IPWidget root({0,0}, {}, IPColor(IPColor::FG::BrightWhite, IPColor::BG::Blue), IPQuad(0,0,0,0), IPQuad(0,0,0,0), IPBoxStyle::Double, IPShadowStyle::None);

    // Horizontal child
    std::vector<IPWidget> horiz_children;
    for (int i = 0; i < 3; ++i) {
        horiz_children.emplace_back("H-" + std::to_string(i), IPColor(IPColor::FG::BrightGreen, IPColor::BG::Default));
    }
    IPWidget horiz(IPWidgetLayout::Horizontal, horiz_children, IPColor(IPColor::FG::BrightYellow, IPColor::BG::Red), IPQuad(0,0,0,0), IPQuad(0,0,0,0), IPBoxStyle::Single, IPShadowStyle::Fill, {2,2});

    // Vertical child
    std::vector<IPWidget> vert_children;
    for (int i = 0; i < 3; ++i) {
        vert_children.emplace_back("V-" + std::to_string(i), IPColor(IPColor::FG::BrightRed, IPColor::BG::Default));
    }
    IPWidget vert(IPWidgetLayout::Vertical, vert_children, IPColor(IPColor::FG::BrightCyan, IPColor::BG::Magenta), IPQuad(0,0,0,0), IPQuad(0,0,0,0), IPBoxStyle::Single, IPShadowStyle::Fill, {20,2});

    // Floating child
    std::vector<IPWidget> floating_children;
    for (int i = 0; i < 2; ++i) {
        IPWidget t("F-" + std::to_string(i), IPColor(IPColor::FG::BrightBlue, IPColor::BG::Default));
        t._xy = {i * 8, i * 2};
        floating_children.push_back(t);
    }
    IPWidget floating({2,10}, floating_children, IPColor(IPColor::FG::BrightMagenta, IPColor::BG::Green), IPQuad(0,0,0,0), IPQuad(0,0,0,0), IPBoxStyle::Single, IPShadowStyle::Fill);

    // Text child
    IPWidget text("Just a text widget!", IPColor(IPColor::FG::BrightWhite, IPColor::BG::Red), IPQuad(0,0,0,0), IPBoxStyle::Single, IPShadowStyle::None);
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
