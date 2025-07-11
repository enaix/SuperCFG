#include <iostream>
#include <vector>
#include <array>

#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
#define IS_POSIX

#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#endif

#include "extra/prettyprint.h"


// Helper for raw input
int getch() {
    #ifdef IS_POSIX
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    int c = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return c;
    #else
    return -1;
    #endif
}

int get_terminal_width() {
    struct winsize w;
    #ifdef IS_POSIX
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) return w.ws_col;
    #endif
    return 80;
}
int get_terminal_height() {
    struct winsize w;
    #ifdef IS_POSIX
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) return w.ws_row;
    #endif
    return 24;
}

/*void test_basic() {
    InteractivePrinter<IPColor> printer(std::cout);

    int term_w = get_terminal_width();
    int term_h = get_terminal_height();
    printer.init_matrix(term_h, term_w);

    // Root widget (Floating)
    IPWidget root({0,0}, {}, IPColor(IPColor::FG::BrightWhite, IPColor::BG::Blue), IPQuad(0,0,0,0), IPQuad(0,0,0,0), IPBoxStyle::Double, IPShadowStyle::None);

    // Horizontal child
    std::vector<IPWidget> horiz_children;
    for (int i = 0; i < 3; ++i) {
        horiz_children.emplace_back("H-" + std::to_string(i), IPColor(IPColor::FG::BrightGreen, IPColor::BG::None));
    }
    IPWidget horiz(IPWidgetLayout::Horizontal, horiz_children, IPColor(IPColor::FG::BrightYellow, IPColor::BG::Red), IPQuad(0,0,0,0), IPQuad(0,0,0,0), IPBoxStyle::Single, IPShadowStyle::Shadow, {2,2});

    // Vertical child
    std::vector<IPWidget> vert_children;
    for (int i = 0; i < 3; ++i) {
        vert_children.emplace_back("V-" + std::to_string(i), IPColor(IPColor::FG::BrightRed, IPColor::BG::None), IPQuad(1,1,1,1));
    }
    IPWidget vert(IPWidgetLayout::Vertical, vert_children, IPColor(IPColor::FG::BrightCyan, IPColor::BG::Magenta), IPQuad(0,0,0,0), IPQuad(0,0,0,0), IPBoxStyle::None, IPShadowStyle::Fill, {20,2});

    // Floating child
    std::vector<IPWidget> floating_children;
    for (int i = 0; i < 2; ++i) {
        IPWidget t("F-" + std::to_string(i), IPColor(IPColor::FG::BrightBlue, IPColor::BG::None));
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
}*/

void test_popup_windows() {
    InteractivePrinter<IPColor> printer(std::cout);
    int term_w = get_terminal_width();
    int term_h = get_terminal_height();
    printer.init_matrix(term_h, term_w);

    // Define a palette
    IPAppStyle<IPColor> style(
        IPColor(IPColor::FG::Default, IPColor::BG::Default), // Primary
        IPColor(IPColor::FG::White, IPColor::BG::Red), // Secondary
        IPColor(IPColor::FG::BrightBlack, IPColor::BG::BrightBlue), // Accent
        IPColor(IPColor::FG::BrightWhite, IPColor::BG::BrightRed), // Selected
        IPColor(IPColor::FG::White, IPColor::BG::Blue), // Inactive
        IPColor(IPColor::FG::White, IPColor::BG::BrightBlue), // Disabled
        IPColor(IPColor::FG::White, IPColor::BG::BrightRed), // BorderActive
        IPColor(IPColor::FG::White, IPColor::BG::Blue), // BorderInactive
        IPColor(IPColor::FG::White, IPColor::BG::BrightBlue) // BorderDisabled
    );

    IPWindow winstack;
    for (int i = 0; i < 3; ++i) {
        IPWidget close_btn("[X]", IPColors::Accent, IPQuad(0,0,0,0), IPBoxStyle::Single, IPShadowStyle::None);
        close_btn.selectable = true;
        close_btn.on_event = [](IPWidget* self, IPWindow* window, const IPEvent& ev) {
            if (ev.type == IPEventType::Select || ev.type == IPEventType::Click) {
                if (window) window->pop();
            }
        };
        IPWidget popup(IPWidgetLayout::Vertical, {
            close_btn,
            IPWidget("Popup window " + std::to_string(i+1), IPColors::Primary, IPQuad(1,1,1,1), IPBoxStyle::None, IPShadowStyle::None)
        }, IPColors::Primary, IPQuad(2*i,2*i,2*i,2*i), IPQuad(1,1,1,1), IPBoxStyle::Double, IPShadowStyle::Shadow, {5*i, 3*i});
        popup.selectable = true;
        popup.on_event = [](IPWidget* self, IPWindow* window, const IPEvent& ev) {
            if (ev.type == IPEventType::OnCreate) {
                if (!self->_children.empty()) self->_children[0].selected = true;
            }
        };
        winstack.push(popup);
    }

    // Main event loop
    while (!winstack.stack.empty()) {
        printer.init_matrix(term_h, term_w);
        for (int i = 0; i < (int)winstack.stack.size(); ++i) {
            bool active = (i == winstack.selector_idx);
            winstack.stack[i].layout();
            winstack.stack[i].render(printer._output_matrix, printer._color_matrix, style, active, 2 + 2*i, 2 + 2*i);
        }
        printer.render_matrix();
        int c = getch();
        if (c == 'q') break;
        if (c == 'x') { winstack.pop(); continue; }
        if (c == 10 || c == ' ') { // Enter or space
            winstack.handle_event(IPEvent(IPEventType::Select));
            continue;
        }
        if (c == 27) { // Escape sequence for arrows
            int c2 = getch();
            if (c2 == '[') {
                int c3 = getch();
                if (c3 == 'A') winstack.handle_event(IPEvent(IPEventType::ArrowUp));
                else if (c3 == 'B') winstack.handle_event(IPEvent(IPEventType::ArrowDown));
                else if (c3 == 'C') winstack.move_selector(1);
                else if (c3 == 'D') winstack.move_selector(-1);
            }
        }
    }
}


int main()
{
    test_popup_windows();
    return 0;
}