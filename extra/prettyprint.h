//
// Created by Flynn on 08.07.2025.
//

#ifndef PRETTYPRINT_H
#define PRETTYPRINT_H

#include "curse.h"

using namespace curse;


class PrettyPrinter
{
protected:
    using TChar = char;
    CurseTerminal<ANSIColor, TChar> terminal;
    AppStyle<ANSIColor> style;
    WindowStack<TChar> winstack;

public:
    PrettyPrinter() : terminal(std::cout), style( // Default style
        ANSIColor(ANSIColor::FG::Default, ANSIColor::BG::Default), // Primary
        ANSIColor(ANSIColor::FG::White, ANSIColor::BG::Red), // Secondary
        ANSIColor(ANSIColor::FG::BrightBlack, ANSIColor::BG::BrightBlue), // Accent
        ANSIColor(ANSIColor::FG::BrightWhite, ANSIColor::BG::BrightRed), // Selected
        ANSIColor(ANSIColor::FG::White, ANSIColor::BG::Blue), // Inactive
        ANSIColor(ANSIColor::FG::White, ANSIColor::BG::BrightBlue), // Disabled
        ANSIColor(ANSIColor::FG::White, ANSIColor::BG::BrightRed), // BorderActive
        ANSIColor(ANSIColor::FG::White, ANSIColor::BG::Blue), // BorderInactive
        ANSIColor(ANSIColor::FG::White, ANSIColor::BG::BrightBlue) // BorderDisabled
    )
    {
        terminal.init_renderer();
        int term_w = terminal.get_terminal_width();
        int term_h = terminal.get_terminal_height();
        terminal.init_matrix(term_h, term_w);
    }


};

`

#endif //PRETTYPRINT_H
