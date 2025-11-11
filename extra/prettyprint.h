//
// Created by Flynn on 08.07.2025.
//

#ifndef PRETTYPRINT_H
#define PRETTYPRINT_H

#include "curse.h"
#include "extra/clang_formatter.h"

#include <cstring>
#include <ctime>
#include <unordered_map>
#include <format>
#include <algorithm>
#include <csignal>
#include <functional>

using namespace curse;



enum class PrinterThemes : std::size_t
{
    // user themes:
    BIOSLight, // default theme for white terminals (OSX)
    BIOSBlue, // default theme (dark terminals)
    // system themes:
    Panic // critical error
};

static constexpr PrinterThemes default_printer_theme = PrinterThemes::BIOSBlue;

static constexpr std::array<std::string, 2> printer_theme_names {
    std::string("bioslight"),
    std::string("biosblue")
};

static constexpr AppStyle<ANSIColor> printer_themes[] = {
    // BIOSLight
    AppStyle<ANSIColor>(ANSIColor(ANSIColor::FG::Default, ANSIColor::BG::Default), // Primary (delimeters)
        ANSIColor(ANSIColor::FG::BrightBlack, ANSIColor::BG::BrightWhite), // Secondary (NTerms)
        ANSIColor(ANSIColor::FG::BrightBlue, ANSIColor::BG::BrightWhite), // Accent 1 (Operators)
        ANSIColor(ANSIColor::FG::BrightGreen, ANSIColor::BG::BrightWhite), // Accent 2 (Terms)
        ANSIColor(ANSIColor::FG::BrightWhite, ANSIColor::BG::BrightBlue), // Accent 3 (highlight)
        ANSIColor(ANSIColor::FG::BrightWhite, ANSIColor::BG::BrightRed), // Selected
        ANSIColor(ANSIColor::FG::White, ANSIColor::BG::BrightBlack), // Inactive (Grouping)
        ANSIColor(ANSIColor::FG::White, ANSIColor::BG::BrightBlue), // Disabled
        ANSIColor(ANSIColor::FG::White, ANSIColor::BG::BrightRed), // BorderActive
        ANSIColor(ANSIColor::FG::White, ANSIColor::BG::Blue), // BorderInactive
        ANSIColor(ANSIColor::FG::White, ANSIColor::BG::BrightBlue)), // BorderDisabled
    // BIOSBlue
    AppStyle<ANSIColor>(ANSIColor(ANSIColor::FG::Default, ANSIColor::BG::Default), // Primary (delimeters)
        ANSIColor(ANSIColor::FG::BrightWhite, ANSIColor::BG::BrightBlack), // Secondary (NTerms)
        ANSIColor(ANSIColor::FG::BrightBlue, ANSIColor::BG::BrightBlack), // Accent 1 (Operators)
        ANSIColor(ANSIColor::FG::BrightGreen, ANSIColor::BG::BrightBlack), // Accent 2 (Terms)
        ANSIColor(ANSIColor::FG::BrightBlue, ANSIColor::BG::BrightBlack), // Accent 3 (highlight)
        ANSIColor(ANSIColor::FG::BrightRed, ANSIColor::BG::BrightWhite), // Selected
        ANSIColor(ANSIColor::FG::White, ANSIColor::BG::BrightBlack), // Inactive (Grouping)
        ANSIColor(ANSIColor::FG::BrightBlack, ANSIColor::BG::Blue), // Disabled
        ANSIColor(ANSIColor::FG::White, ANSIColor::BG::BrightRed), // BorderActive
        ANSIColor(ANSIColor::FG::Black, ANSIColor::BG::BrightBlue), // BorderInactive
        ANSIColor(ANSIColor::FG::BrightBlack, ANSIColor::BG::Blue)), // BorderDisabled
    // Panic
    AppStyle<ANSIColor>(ANSIColor(ANSIColor::FG::Default, ANSIColor::BG::Default), // Primary (delimeters)
        ANSIColor(ANSIColor::FG::BrightBlack, ANSIColor::BG::BrightWhite), // Secondary (NTerms)
        ANSIColor(ANSIColor::FG::BrightBlack, ANSIColor::BG::BrightWhite), // Accent 1 (Operators)
        ANSIColor(ANSIColor::FG::BrightBlack, ANSIColor::BG::BrightWhite), // Accent 2 (Terms)
        ANSIColor(ANSIColor::FG::BrightWhite, ANSIColor::BG::Red), // Accent 3 (highlight)
        ANSIColor(ANSIColor::FG::BrightWhite, ANSIColor::BG::BrightRed), // Selected
        ANSIColor(ANSIColor::FG::White, ANSIColor::BG::BrightWhite), // Inactive (Grouping)
        ANSIColor(ANSIColor::FG::White, ANSIColor::BG::BrightRed), // Disabled
        ANSIColor(ANSIColor::FG::White, ANSIColor::BG::BrightRed), // BorderActive
        ANSIColor(ANSIColor::FG::White, ANSIColor::BG::Red), // BorderInactive
        ANSIColor(ANSIColor::FG::White, ANSIColor::BG::BrightRed)), // BorderDisabled
};


static constexpr BoxStyle printer_theme_box_style[] = {
    DoubleBoxStyle, // bioslight
    PlainBoxStyle,  // biosblue
    PlainBoxStyle,  // panic
};



enum class PrinterMode : std::size_t
{
    Normal,
    Open,
    Theme,
    Move
};


enum class PrinterWindows
{
    Stack,
    Descend,
    AST
};


class PrettyPrinter
{
protected:
    using TChar = char;
    CurseTerminal<ANSIColor, TChar> terminal;
    AppStyle<ANSIColor> style;
    WindowStack<TChar> winstack;
    std::unordered_map<PrinterWindows, std::size_t> window_id;
    PrinterMode _mode;

    // pre-rendered widgets
    Widget<TChar> _ebnf;
    Widget<TChar> _rr_tree;
    Widget<TChar> _fix;
    // current selection
    PrinterThemes _style_select;
    std::basic_string<TChar> _style_name;
    BoxStyle _cur_box_style; // depends on the current theme

public:
    PrettyPrinter() : terminal(std::cout), style(printer_themes[(std::size_t)default_printer_theme]), _mode(PrinterMode::Normal), _style_select(PrinterThemes::Panic), _cur_box_style(printer_theme_box_style[(std::size_t)default_printer_theme])
    {
        terminal.init_renderer();
        std::srand(std::time({}));
    }

    ~PrettyPrinter() { close(); }

    static void close() { CurseTerminal<ANSIColor, char>::restore_terminal(); }

    template<class RRTree, class RulesDef>
    void init_windows(const RRTree& rr_tree, const RulesDef& rules)
    {
        int term_w = terminal.get_terminal_width();
        int term_h = terminal.get_terminal_height();
        terminal.init_matrix(term_h, term_w);

        window_id.insert({PrinterWindows::Stack, 0});
        winstack.push(Widget<TChar>()); // STACK
        window_id.insert({PrinterWindows::Descend, 1});
        winstack.push(Widget<TChar>()); // DESCEND
        window_id.insert({PrinterWindows::AST, 2});
        winstack.push(make_ast(TreeNode<std::basic_string<TChar>>())); // AST

        winstack.push_overlay(make_bottom_overlay());
        set_bottom_overlay_pos();
        winstack.selector_idx = 0;

        _ebnf = make_ebnf_preview(rules);
        _rr_tree = make_rr_tree(rr_tree);
        _fix = make_empty_fix();
    }


    template<class TMatches, class TRules, class AllTerms, class NTermsPosPairs, class TermsPosPairs>
    void init_ctx_classes(const TMatches& rules, const TRules& all_rr, const AllTerms& all_t, const NTermsPosPairs& pairs_nt, const TermsPosPairs& pairs_t)
    {
        _fix = make_rules_fix(rules, all_rr, all_t, pairs_nt, pairs_t);
    }

    bool process() // Returns true if the stack can progress further
    {
        //return true; // useful for debugging
        //if (c == 'q') return false;

        terminal.reset_output_matrix();
        winstack.render_all(terminal._output_matrix, terminal._color_matrix, style);
        winstack.render_overlays(terminal._output_matrix, terminal._color_matrix, style);
        terminal.render_matrix();
        // HANDLE OVERLAY

        int c = terminal.getch(), last_char = '\0';
        while (true) {
            bool handled = handle_mode(c, last_char);
            set_bottom_overlay_pos();
            last_char = c; // save last char
            terminal.reset_output_matrix();
            winstack.render_all(terminal._output_matrix, terminal._color_matrix, style);
            winstack.render_overlays(terminal._output_matrix, terminal._color_matrix, style);
            terminal.render_matrix();
            if (!handled)
                break;
            c = terminal.getch();
        }
        //if (handled)
        //    return false; // continue

        if (c == 10 || c == ' ')
        {
            // Enter or space
            if (!winstack.handle_event(IPEvent(EventType::Select)))
            {
                if (winstack.selector_idx == 0 || winstack.selector_idx == 1) // Very crude check for current window
                    return true;
            }
        }
        if (c == 27)
        {
            // Escape sequence for arrows
            int c2 = terminal.getch();
            if (c2 == '[')
            {
                int c3 = terminal.getch();
                if (c3 == 'A') winstack.handle_event(IPEvent(EventType::ArrowUp));
                else if (c3 == 'B') winstack.handle_event(IPEvent(EventType::ArrowDown));
                else if (c3 == 'C') winstack.handle_event(IPEvent(EventType::ArrowRight));
                else if (c3 == 'D') winstack.handle_event(IPEvent(EventType::ArrowLeft));
            }
        }
        if (c == '\t') winstack.move_selector_tab(1);
        return false;
    }

    // Stack rendering
    template<class VStr, class TokenTSet, class TokenType>
    void update_stack(const std::vector<GrammarSymbol<VStr, TokenTSet>>& stack, const std::vector<ConstVec<TokenType>>& related_types, const ConstVec<TokenType>& intersect, std::size_t idx)
    {
        const auto& id = window_id.find(PrinterWindows::Stack);
        if (id != window_id.end())
            winstack.stack[id->second].refresh(make_stack(stack, related_types, intersect, idx));
    }

    template<class VStr, class TokenTSet, class TSymbol>
    void update_descend(const std::vector<GrammarSymbol<VStr, TokenTSet>>& stack, const TSymbol& rule, std::size_t idx, std::size_t candidate, std::size_t total, std::size_t parsed, bool found)
    {
        const auto& id = window_id.find(PrinterWindows::Descend);
        if (id != window_id.end())
            winstack.stack[id->second].refresh(make_descend(stack, rule, idx, candidate, total, parsed, found));
    }

    template<class Tree>
    void update_ast(const Tree& tree)
    {
        const auto& id = window_id.find(PrinterWindows::AST);
        if (id != window_id.end())
            winstack.stack[id->second].refresh(make_ast(tree));
    }

    void set_empty_descend()
    {
        const auto& id = window_id.find(PrinterWindows::Descend);
        if (id == window_id.end())
            return;
        winstack.stack[id->second].refresh(Widget<TChar>(WidgetLayout::Vertical, {
            Widget<TChar>(std::basic_string<TChar>("DESCEND"), Colors::Primary, Quad(1,0,1,0)),
            Widget<TChar>(std::basic_string<TChar>("<empty>"), Colors::Primary, Quad(1,0,1,0))
        }, Colors::None, Quad(), Quad(1,1,1,1), &_cur_box_style));
    }

    // Blocking !!
    [[noreturn]] void guru_meditation(const char* message, const char* file, int line)
    {
        apply_theme(PrinterThemes::Panic);
        winstack.push(make_guru(message, file, line), (std::size_t)IPWindowFlags::Modal);
        // block input
        while (true) { process(); } // will shut down when user presses abort
    }


protected:
    void apply_theme(PrinterThemes theme)
    {
        const std::size_t thm = static_cast<std::size_t>(theme);
        style = printer_themes[thm];
        _cur_box_style = printer_theme_box_style[thm];
    }

    template<class TSymbol>
    static Widget<TChar> make_nterm(const TSymbol& s)
    {
        Widget<TChar> nterm(std::basic_string<TChar>(s.name.c_str()), Colors::Secondary);
        return nterm;
    }

    template<class TSymbol>
    static Widget<TChar> make_term(const TSymbol& s)
    {
        Widget<TChar> term('\"' + std::basic_string<TChar>(s.name.c_str()) + '\"', Colors::Accent2);
        return term;
    }

    template<class TSymbol>
    static Widget<TChar> make_symbol(const TSymbol& s)
    {
        if constexpr (is_nterm<TSymbol>())
            return make_nterm(s);
        else {
            if constexpr (is_term<TSymbol>())
                return make_term(s);
            else
                return make_terms_range(s);
        }
    }

    template<class VStr, class TokenTSet>
    static Widget<TChar> make_token(const GrammarSymbol<VStr, TokenTSet>& s)
    {
        if (s.is_token())
            return Widget<TChar>(std::basic_string<TChar>('\"' + std::basic_string<TChar>(s.value)) + '\"', Colors::Accent2);
        return Widget<TChar>(std::basic_string<TChar>(std::basic_string<TChar>(s.type.front())), Colors::Secondary);
    }

    template<class TSymbol>
    static Widget<TChar> make_terms_range(const TSymbol& s)
    {
        return Widget<TChar>(WidgetLayout::Horizontal, {
            Widget<TChar>(std::basic_string<TChar>("["), Colors::Accent),
            Widget<TChar>('\'' + std::basic_string<TChar>(s.start.c_str()) + '\'', Colors::Accent2),
            Widget<TChar>(std::basic_string<TChar>("-"), Colors::Accent),
            Widget<TChar>('\'' + std::basic_string<TChar>(s.end.c_str()) + '\'', Colors::Accent2),
            Widget<TChar>(std::basic_string<TChar>("]"), Colors::Accent)
        }, Colors::None);
    }

    template<OpType type>
    static Widget<TChar> make_bnf_symbol_multi()
    {
        switch (type)
        {
        case OpType::Concat:
            return Widget<TChar>(std::basic_string<TChar>(" + "), Colors::Accent);
        case OpType::Alter:
            return Widget<TChar>(std::basic_string<TChar>(" | "), Colors::Accent);
        default:
            static_assert(type == OpType::Concat || type == OpType::Alter, "OpType must take multiple arguments");
            return {}; // Err
        }
    }

    template<OpType type>
    static Widget<TChar> make_bnf_symbol_left()
    {
        switch (type)
        {
        case OpType::Optional:
            return Widget<TChar>(std::basic_string<TChar>("["), Colors::Accent);
        case OpType::Repeat:
            return Widget<TChar>(std::basic_string<TChar>("{"), Colors::Accent);
        case OpType::Group:
            return Widget<TChar>(std::basic_string<TChar>("("), Colors::Accent);
        default:
            static_assert(type != OpType::Concat && type != OpType::Alter, "OpType must take a single argument");
            return {}; // Err
        }
    }

    template<OpType type>
    static Widget<TChar> make_bnf_symbol_right()
    {
        switch (type)
        {
        case OpType::Optional:
            return Widget<TChar>(std::basic_string<TChar>("]"), Colors::Accent);
        case OpType::Repeat:
            return Widget<TChar>(std::basic_string<TChar>("}*"), Colors::Accent);
        case OpType::Group:
            return Widget<TChar>(std::basic_string<TChar>(")"), Colors::Accent);
        default:
            static_assert(type != OpType::Concat && type != OpType::Alter, "OpType must take a single argument");
            return {}; // Err
        }
    }

    template<class TSymbol>
    static Widget<TChar> make_bnf_symbol_ext_repeat(const TSymbol& s)
    {
        if constexpr (get_operator<TSymbol>() == OpType::RepeatExact)
        {
            return Widget<TChar>(WidgetLayout::Horizontal, {
                Widget<TChar>(std::basic_string<TChar>("{"), Colors::Accent),
                Widget<TChar>(std::basic_string<TChar>(make_recurse_op(std::get<0>(s.terms))), Colors::Accent),
                Widget<TChar>(std::basic_string<TChar>(":"), Colors::Accent),
                Widget<TChar>(std::basic_string<TChar>(std::to_string(get_repeat_times(s))), Colors::Accent),
                Widget<TChar>(std::basic_string<TChar>("}*"), Colors::Accent)
            }, Colors::None);
        } else {
            Widget<TChar> range(WidgetLayout::Horizontal, {
                Widget<TChar>(std::basic_string<TChar>("{"), Colors::Accent),
                Widget<TChar>(std::basic_string<TChar>(make_recurse_op(std::get<0>(s.terms))), Colors::Accent),
                Widget<TChar>(std::basic_string<TChar>(":"), Colors::Accent),
            }, Colors::None);
            range.add_child(Widget<TChar>(std::basic_string<TChar>("["), Colors::Accent));
            if constexpr (get_operator<TSymbol>() == OpType::RepeatRange)
            {
                range.add_child(Widget<TChar>(std::basic_string<TChar>(std::to_string(get_range_from(s))), Colors::Accent));
                range.add_child(Widget<TChar>(std::basic_string<TChar>(","), Colors::Accent));
                range.add_child(Widget<TChar>(std::basic_string<TChar>(std::to_string(get_range_to(s))), Colors::Accent));
                range.add_child(Widget<TChar>(std::basic_string<TChar>("]"), Colors::Accent));
            } else { // RepeatGE
                range.add_child(Widget<TChar>(std::basic_string<TChar>(std::to_string(get_repeat_times(s))), Colors::Accent));
                range.add_child(Widget<TChar>(std::basic_string<TChar>(","), Colors::Accent));
                range.add_child(Widget<TChar>(std::basic_string<TChar>("inf"), Colors::Accent));
                range.add_child(Widget<TChar>(std::basic_string<TChar>(")"), Colors::Accent));
            }
            range.add_child(Widget<TChar>(std::basic_string<TChar>("}*"), Colors::Accent));
            return range;
        }
    }

    template<class TSymbol>
    static Widget<TChar> make_recurse_op(const TSymbol& s)
    {
        Widget<TChar> op(WidgetLayout::Horizontal, {}, Colors::None);
        make_recurse_op(s, [&op](auto wid){ op.add_child(wid); });
        return op;
    }

    template<class TSymbol>
    static void make_recurse_op(const TSymbol& s, auto push)
    {
        if constexpr (is_operator<TSymbol>())
        {
            if constexpr (get_operator<TSymbol>() == OpType::Concat || get_operator<TSymbol>() == OpType::Alter)
            {
                push(Widget<TChar>(std::basic_string<TChar>("("), Colors::Inactive));
                tuple_each(s.terms, [&](std::size_t i, const auto& symbol){
                    push(make_recurse_op(symbol));
                    if (i < std::tuple_size_v<std::decay_t<decltype(s.terms)>> - 1)
                        push(make_bnf_symbol_multi<get_operator<TSymbol>()>());
                });
                push(Widget<TChar>(std::basic_string<TChar>(")"), Colors::Inactive));
            }
            else if constexpr (get_operator<TSymbol>() == OpType::Optional || get_operator<TSymbol>() == OpType::Repeat || get_operator<TSymbol>() == OpType::Group)
            {
                push(Widget<TChar>(std::basic_string<TChar>("("), Colors::Inactive));
                push(make_bnf_symbol_left<get_operator<TSymbol>()>());
                push(make_recurse_op(std::get<0>(s.terms)));
                push(make_bnf_symbol_right<get_operator<TSymbol>()>());
                push(Widget<TChar>(std::basic_string<TChar>(")"), Colors::Inactive));
            }
            else if constexpr (get_operator<TSymbol>() == OpType::Except)
            {
                push(Widget<TChar>(std::basic_string<TChar>("("), Colors::Inactive));
                push(Widget<TChar>(std::basic_string<TChar>("("), Colors::Accent));
                push(make_recurse_op(std::get<0>(s.terms)));
                push(Widget<TChar>(std::basic_string<TChar>("-"), Colors::Accent));
                push(make_recurse_op(std::get<1>(s.terms)));
                push(Widget<TChar>(std::basic_string<TChar>(")"), Colors::Accent));
                push(Widget<TChar>(std::basic_string<TChar>(")"), Colors::Inactive));
            }
            else if constexpr (get_operator<TSymbol>() == OpType::End)
            {
                push(make_recurse_op(std::get<0>(s.terms)));
                push(Widget<TChar>(std::basic_string<TChar>(";"), Colors::Inactive));
            }
            else // Repeat* operators
                push(make_bnf_symbol_ext_repeat(s));
        }
        else if constexpr (is_nterm<TSymbol>())
        {
            push(make_nterm(s));
        }
        else
        {
            if constexpr (is_term<TSymbol>())
                push(make_term(s));
            else
                push(make_terms_range(s));
        }
    }

    template<class RRTree>
    Widget<TChar> make_rr_tree(const RRTree& rr_tree)
    {
        // Grid containing all columns
        Widget<TChar> rr_grid(WidgetLayout::Horizontal, {
            Widget<TChar>(WidgetLayout::Vertical, {}, Colors::None), // NTerm
            Widget<TChar>(WidgetLayout::Vertical, {}, Colors::None), // ->
            Widget<TChar>(WidgetLayout::Vertical, {}, Colors::None) // Terms
        }, Colors::None, Quad(), Quad(1,0,1,0));

        constexpr std::size_t n = std::tuple_size_v<std::decay_t<decltype(rr_tree.defs)>>;
        // Iterate over each def and populate the first column
        tuple_each(rr_tree.defs, [&](std::size_t i, const auto& elem){
            rr_grid._children[0].add_child(make_nterm(std::get<0>(elem.terms)));
        });

        for (std::size_t i = 0; i < n; i++)
            rr_grid._children[1].add_child(Widget<TChar>(std::basic_string<TChar>("->"), Colors::Primary));

        // Iterate over each def and over all related terms
        tuple_each(rr_tree.tree, [&](std::size_t i, const auto& elem){
            Widget<TChar>& cell = rr_grid._children[2].add_child(Widget<TChar>(WidgetLayout::Horizontal, {}, Colors::None, Quad(), Quad(1,0,1,0)));
            tuple_each(elem, [&](std::size_t j, const auto& nterm){
                rr_grid._children[2]._children[i].add_child(make_nterm(nterm));
            });
        });
        // Create wrapper
        return Widget<TChar>(WidgetLayout::Vertical, {
            Widget<TChar>(std::basic_string<TChar>("REVERSE RULES TREE"), Colors::Primary, Quad(1,0,1,0)),
            rr_grid
        }, Colors::None, Quad(), Quad(1,1,1,1), &_cur_box_style);
    }

    Widget<TChar> make_empty_fix()
    {
        return Widget<TChar>(WidgetLayout::Vertical, {
            Widget<TChar>(std::basic_string<TChar>("-FIX Heuristic"), Colors::Primary, Quad(1,0,1,0)),
            Widget<TChar>(std::basic_string<TChar>("Heuristic context manager is not initialized"), Colors::Secondary)
        }, Colors::None, Quad(), Quad(1,1,1,1), &_cur_box_style);
    }

    template<class TMatches, class TRules, class AllTerms, class NTermsPosPairs, class TermsPosPairs>
    Widget<TChar> make_rules_fix(const TMatches& rules, const TRules& all_rr, const AllTerms& all_t, const NTermsPosPairs& pairs_nt, const TermsPosPairs& pairs_t)
    {
        // Each of these classes contains an element and its position in a rule

        #ifdef ENABLE_SUPERCFG_DIAG
        SuperCFGDiagnostics::get().print_template_type(nt_pairs, "[NTerm cached class]");
        #endif
        /*
         *  HBox
         * _______
         * |V|V|V|
         * | | | |
         * |_|_|_|
         */
        Widget<TChar> rr_grid(WidgetLayout::Horizontal, {
            Widget<TChar>(WidgetLayout::Vertical, {Widget<TChar>()}, Colors::None), // NTerm
            Widget<TChar>(WidgetLayout::Vertical, {Widget<TChar>()}, Colors::None), // ->
            Widget<TChar>(WidgetLayout::Vertical, {Widget<TChar>(std::basic_string<TChar>("PREFIX"), Colors::Secondary)}, Colors::None), // Prefix
            Widget<TChar>(WidgetLayout::Vertical, {Widget<TChar>()}, Colors::None), // Sep
            Widget<TChar>(WidgetLayout::Vertical, {Widget<TChar>(std::basic_string<TChar>("POSTFIX"), Colors::Secondary)}, Colors::None), // Postfix
            Widget<TChar>(WidgetLayout::Vertical, {Widget<TChar>(std::basic_string<TChar>("MAX PRE"), Colors::Secondary)}, Colors::None), // Postfix
            Widget<TChar>(WidgetLayout::Vertical, {Widget<TChar>(std::basic_string<TChar>("MIN POST"), Colors::Secondary)}, Colors::None), // Postfix
        }, Colors::None, Quad(), Quad(1,0,1,0));

        // Initialize the grid with empty cells
        tuple_each(rules, [&](std::size_t i, const auto& rule){
            rr_grid.at(0).add_child(make_nterm(rule));//(std::get<0>(rule.terms)));
            rr_grid.at(1).add_child(Widget<TChar>(std::basic_string<TChar>("->")));
            rr_grid.at(2).add_child(Widget<TChar>(WidgetLayout::Horizontal, { Widget<TChar>("-") }, Colors::None)); // pre (should be an hbox)
            rr_grid.at(3).add_child(Widget<TChar>(std::basic_string<TChar>(":"))); // sep
            rr_grid.at(4).add_child(Widget<TChar>(WidgetLayout::Horizontal, { Widget<TChar>("-") }, Colors::None)); // post (should be an hbox)
            rr_grid.at(5).add_child(Widget<TChar>(WidgetLayout::Horizontal, { Widget<TChar>("-") }, Colors::None)); // max pre (should be an hbox)
            rr_grid.at(6).add_child(Widget<TChar>(WidgetLayout::Horizontal, { Widget<TChar>("-") }, Colors::None)); // min post (should be an hbox)
        });

        auto populate_grid = [&]<bool is_nt>(const auto& symbol_pairs){
            tuple_each_idx(symbol_pairs, [&]<std::size_t i>(const auto& elem){
                const auto& symbol = [&](){
                    if constexpr (is_nt)
                        // fetch from rules
                        return std::get<i>(rules);
                    else
                        return std::get<i>(all_t);
                }();

                const auto& [related_types, fix_limits] = elem;

                auto process = [&,fix_limits](std::size_t j, const auto& pack){
                    using max_t = std::integral_constant<std::size_t, std::numeric_limits<std::size_t>::max()>;
                    const auto& [rule, fix] = pack;
                    const auto [pre, post_dist] = fix;
                    const auto [max_pre, min_post] = fix_limits; // Get max prefix and min postfix in this rule
                    const auto post = post_dist; // Convert post from the distance to the end to an id

                    constexpr std::size_t row = tuple_index_of<TMatches, decltype(rule)>();
                    if constexpr (row == std::numeric_limits<std::size_t>::max())
                    {
                        rr_grid.at(0).add_child(make_nterm(rule));
                        rr_grid.at(1).add_child(Widget<TChar>(std::basic_string<TChar>("->"), Colors::Secondary));
                        return;
                    }
                    // find grid positions

                    if (pre != max_t())
                    {
                        //std::cout << "prefix : at " << row + 1 << " : symbol " << make_symbol(symbol)._content << " at " << pre << std::endl;
                        // initialize grid for the prefix
                        for (int l = rr_grid.at(2).at(row+1)._children.size(); l <= pre; l++)
                            rr_grid.at(2).at(row+1).add_child(Widget<TChar>(std::basic_string<TChar>("?"), Colors::Accent3));
                        rr_grid.at(2).at(row+1).at(pre).refresh(make_symbol(symbol));

                        // Repeating pattern
                        if (is_nt && pre % 2 != 0)
                            rr_grid.at(2).at(row+1).at(pre)._color = Colors::Primary;
                    }

                    if (post_dist != max_t())
                    {
                        //std::cout << "postfix : at " << row + 1 << " : symbol " << make_symbol(symbol)._content << " at " << post << "(min_post : " << min_post << ")" << std::endl;
                        // initialize grid for the postfix
                        for (int l = rr_grid.at(4).at(row+1)._children.size(); l <= post; l++)
                            rr_grid.at(4).at(row+1).add_child(Widget<TChar>(std::basic_string<TChar>("?"), Colors::Accent3));
                        rr_grid.at(4).at(row+1).at(post).refresh(make_symbol(symbol));

                        // Repeating pattern
                        if (is_nt && post % 2 != 0)
                            rr_grid.at(4).at(row+1).at(post)._color = Colors::Primary;
                    }

                    // add limits
                    if (max_pre != 0)
                        rr_grid.at(5).at(row+1).at(0).set_text(std::basic_string<TChar>(std::to_string(max_pre)));
                    if (min_post != 0)
                        rr_grid.at(6).at(row+1).at(0).set_text(std::basic_string<TChar>(std::to_string(min_post)));
                };

                tuple_each(related_types, process);
            });
        };
        populate_grid.template operator()<true>(pairs_nt);
        populate_grid.template operator()<false>(pairs_t);

        // quick fix: reverse postfixes
        for (std::size_t i = 0; i < rr_grid.at(4)._children.size(); i++)
        {
            std::reverse(rr_grid.at(4).at(i)._children.begin(), rr_grid.at(4).at(i)._children.end());
        }
        //std::cin.ignore();
        // Create wrapper
        return Widget<TChar>(WidgetLayout::Vertical, {
            Widget<TChar>(std::basic_string<TChar>("-FIX Heuristic"), Colors::Primary, Quad(1,0,1,0)),
            rr_grid
        }, Colors::None, Quad(), Quad(1,1,1,1), &_cur_box_style);
    }

    template<class RulesDef>
    Widget<TChar> make_ebnf_preview(const RulesDef& rules)
    {
        // Grid containing all rules
        Widget<TChar> rr_grid(WidgetLayout::Horizontal, {
            Widget<TChar>(WidgetLayout::Vertical, {}, Colors::None), // NTerm
            Widget<TChar>(WidgetLayout::Vertical, {}, Colors::None), // :=
            Widget<TChar>(WidgetLayout::Vertical, {}, Colors::None) // Rule
        }, Colors::None, Quad(), Quad(1,0,1,0));

        constexpr std::size_t n = std::tuple_size_v<std::decay_t<decltype(rules.terms)>>;
        // Iterate over each def and populate the first column
        tuple_each(rules.terms, [&](std::size_t i, const auto& elem){
            rr_grid._children[0].add_child(make_nterm(std::get<0>(elem.terms)));
        });

        for (std::size_t i = 0; i < n; i++)
            rr_grid._children[1].add_child(Widget<TChar>(std::basic_string<TChar>(":="), Colors::Primary));

        // Iterate over each def and over all related terms
        tuple_each(rules.terms, [&](std::size_t i, const auto& elem){
            rr_grid._children[2].add_child(make_recurse_op(std::get<1>(elem.terms)));
        });

        // Create wrapper
        return Widget<TChar>(WidgetLayout::Vertical, {
            Widget<TChar>(std::basic_string<TChar>("EBNF Grammar"), Colors::Primary, Quad(1,0,1,0)),
            rr_grid
        }, Colors::None, Quad(), Quad(1,1,1,1), &_cur_box_style);
    }

    template<class VStr, class TokenTSet, class TokenType>
    Widget<TChar> make_stack(const std::vector<GrammarSymbol<VStr, TokenTSet>>& stack, const std::vector<ConstVec<TokenType>>& related_types, const ConstVec<TokenType>& intersect, std::size_t idx)
    {
        Widget<TChar> stack_box(WidgetLayout::Horizontal, {
            Widget<TChar>(WidgetLayout::Vertical, {
                Widget<TChar>(std::basic_string<TChar>("stack"), Colors::Accent),
                Widget<TChar>(std::basic_string<TChar>("reltype"), Colors::Accent),
                // ...
            }, Colors::None), // First row
        }, Colors::None, Quad(), Quad(1,0,1,0));

        // Get max number of related types
        std::size_t maxlen = 0;
        for (const auto& types : related_types)
            if (types.size() > maxlen)
                maxlen = types.size();

        // Populate left column
        for (std::size_t i = 1; i < maxlen; i++)
            stack_box.front().add_child(Widget<TChar>(std::basic_string<TChar>("      -"), Colors::Accent));

        for (std::size_t i = 0; i < stack.size(); i++)
        {
            // Add column
            stack_box.add_child(Widget<TChar>(WidgetLayout::Vertical, {
                make_token(stack[i])
            }, Colors::None));

            for (std::size_t j = 0; j < maxlen; j++)
            {
                if (j < related_types[i].size())
                    stack_box.back().add_child(Widget<TChar>(std::basic_string<TChar>(related_types[i][j]), Colors::Secondary));
                else
                    stack_box.back().add_child(Widget<TChar>(std::basic_string<TChar>(" "), Colors::None));
            }
            if (i >= idx) // Take only rhs slices of the stack
                stack_box.back().add_child(Widget<TChar>(std::basic_string<TChar>("*"), Colors::Accent3)); // Add bottom highlight widget
            else
                stack_box.back().add_child(Widget<TChar>(std::basic_string<TChar>("-"), Colors::None));
        }

        // Add emptyset sign
        if (intersect.size() == 0)
            stack_box.front().add_child(Widget<TChar>("?", Colors::Secondary));
        else
            for (std::size_t i = 0; i < intersect.size(); i++)
                stack_box.front().add_child(Widget<TChar>(std::basic_string<TChar>(intersect[i]), Colors::Secondary));

        // Create wrapper
        return Widget<TChar>(WidgetLayout::Vertical, {
            Widget<TChar>(std::basic_string<TChar>("SR PARSER ROUTINE"), Colors::Primary, Quad(1,0,1,0)),
            stack_box
        }, Colors::None, Quad(), Quad(1,1,1,1), &_cur_box_style);
    }

    Widget<TChar> make_guru(const char* message, const char* file, int line)
    {
        Widget<TChar> alert(WidgetLayout::Vertical, {
            Widget<TChar>(std::basic_string<TChar>("GURU MEDITATION"), Colors::Primary, Quad(1,0,1,0)),
            Widget<TChar>(std::basic_string<TChar>(message), Colors::Primary, Quad(1,0,1,0)),
            Widget<TChar>(std::basic_string<TChar>("at ") + std::basic_string<TChar>(file) + ':' + std::basic_string<TChar>(std::to_string(line)), Colors::Primary, Quad(1,0,1,0)),
        }, Colors::None, Quad(), Quad(1,1,1,1), &_cur_box_style);

        Widget<TChar> abort_btn(std::basic_string<TChar>("<ABORT>"), Colors::Primary, Quad(1,1,1,1));
        abort_btn.set_selectable(true);
        abort_btn.on_event = [](Widget<TChar>* self, WindowStack<TChar>* window, const IPEvent& ev,
                                         const std::vector<int>& path) -> bool
        {
            if (ev.type == EventType::Select || ev.type == EventType::Click)
            {
                close(); // Return to regular input mode
                exit(1); // Abort
            }
            return false;
        };
        alert.add_child(abort_btn);
        return alert;
    }

    template<class VStr, class TokenTSet, class TSymbol>
    Widget<TChar> make_descend(const std::vector<GrammarSymbol<VStr, TokenTSet>>& stack, const TSymbol& rule, std::size_t idx, std::size_t candidate, std::size_t total, std::size_t parsed, bool found)
    {
        Widget<TChar> rr_grid(WidgetLayout::Horizontal, { // we have to populate the first row with a spacer for the status row
            Widget<TChar>(WidgetLayout::Vertical, { Widget<TChar>(), make_nterm(std::get<0>(rule)) }, Colors::None)
        }, Colors::None, Quad(), Quad(1,0,1,0));

        // Make status row
        if (found)
            rr_grid.front().front().refresh(Widget<TChar>(std::basic_string<TChar>("[OK]"), Colors::Accent2));
        else
            rr_grid.front().front().refresh(Widget<TChar>(std::basic_string<TChar>("[--]"), Colors::Primary));

        make_recurse_op(std::get<1>(rule), [&](auto wid){
            rr_grid.add_child(Widget<TChar>(WidgetLayout::Vertical, { Widget<TChar>(), wid }, Colors::None));
        }); // populate the grid

        for (std::size_t i = rr_grid.widgets_num(); i < 3; i++)
            rr_grid.add_child(Widget<TChar>(WidgetLayout::Vertical, { Widget<TChar>(), Widget<TChar>() }, Colors::None));

        rr_grid.at(1).front().refresh(Widget<TChar>(std::basic_string<TChar>("CND"), Colors::Primary));
        rr_grid.at(2).front().refresh(Widget<TChar>(std::basic_string<TChar>(std::format("{}/{}", candidate+1, total)), Colors::Primary));

        for (std::size_t i = idx; i < stack.size(); i++)
        {
            const std::size_t cell_pos = i - idx + 1;
            if (cell_pos >= rr_grid.widgets_num())
            {
                rr_grid.add_child(Widget<TChar>(WidgetLayout::Vertical, {Widget<TChar>(std::basic_string<TChar>(""))}));
            }

            // spacer
            rr_grid._children[cell_pos].add_child(Widget<TChar>(std::basic_string<TChar>("")));

            rr_grid._children[cell_pos].add_child(make_token(stack[i]));

            if (i <= parsed)
                rr_grid._children[cell_pos].add_child(Widget<TChar>(std::basic_string<TChar>("*"), Colors::Accent3)); // Add bottom highlight widget
            else
                rr_grid._children[cell_pos].add_child(Widget<TChar>(std::basic_string<TChar>("-"), Colors::None));
        }

        // Create wrapper
        return Widget<TChar>(WidgetLayout::Vertical, {
            Widget<TChar>(std::basic_string<TChar>("DESCEND"), Colors::Primary, Quad(1,0,1,0)),
            rr_grid
        }, Colors::None, Quad(), Quad(1,1,1,1), &_cur_box_style);
    }

    template<class Tree>
    Widget<TChar> make_ast(const Tree& ast_root)
    {
        // Create the main container for the AST
        Widget<TChar> ast_container(WidgetLayout::Vertical, {
                Widget<TChar>(std::basic_string<TChar>("ABS SYNTAX TREE"), Colors::Primary, Quad(1,0,1,0))
            }, Colors::None, Quad(), Quad(1,1,1,1), &_cur_box_style);

        // Helper function to recursively build the tree representation
        std::function<Widget<TChar>(const Tree&, std::size_t, bool, const std::string&)> build_tree_node =
            [&](const Tree& node, std::size_t depth, bool is_last, const std::string& prefix) -> Widget<TChar> {

            Widget<TChar> node_widget(WidgetLayout::Vertical, {}, Colors::None);

            // Create the tree structure prefix
            std::string tree_prefix = prefix;
            if (depth > 0) {
                tree_prefix += is_last ? "+--" : "|--";
            }

            // Create the node display
            Widget<TChar> node_line(WidgetLayout::Horizontal, {}, Colors::None);

            // Add tree structure
            if (!tree_prefix.empty()) {
                node_line.add_child(Widget<TChar>(std::basic_string<TChar>(tree_prefix), Colors::Primary));
            }

            // Add node name
            if (!node.name.empty()) {
                node_line.add_child(Widget<TChar>(std::basic_string<TChar>(node.name.c_str()), Colors::Secondary));
            }

            // Add node value if present (for terminal nodes)
            if (!node.value.empty()) {
                node_line.add_child(Widget<TChar>(std::basic_string<TChar>(" : "), Colors::Accent));
                node_line.add_child(Widget<TChar>(std::basic_string<TChar>(std::basic_string<TChar>("\"") + std::basic_string<TChar>(node.value.c_str()) + "\""), Colors::Accent2));
            }

            node_widget.add_child(node_line);

            // Process children
            for (std::size_t i = 0; i < node.nodes.size(); ++i) {
                bool child_is_last = (i == node.nodes.size() - 1);
                std::string child_prefix = prefix;
                if (depth > 0) {
                    child_prefix += is_last ? "    " : "|   ";
                }

                Widget<TChar> child_widget = build_tree_node(node.nodes[i], depth + 1, child_is_last, child_prefix);
                node_widget.add_child(child_widget);
            }

            return node_widget;
        };

        // Build the tree starting from root
        Widget<TChar> tree_content = build_tree_node(ast_root, 0, true, "");
        ast_container.add_child(tree_content);

        return ast_container;
    }



    // ==========
    // MENU LOGIC
    //   You may edit keybinds here
    // ==========

    bool handle_mode(const char input, int last_char) // handled = true means that we shouldn't handle the event further
    {
        if (_mode == PrinterMode::Normal && last_char == 17) [[unlikely]]
            {
                if (input == 10 || input == 'y' || input == 'Y')
                    {
                        close();
                        exit(0);
                    }
                winstack.overlays[0] = make_bottom_overlay();
                return true;
            }

        switch (_mode)
            {
            case PrinterMode::Normal:
                switch (input)
                    {
                    case 15: // ^O
                        _mode = PrinterMode::Open;
                        winstack.overlays[0] = make_bottom_overlay();
                        return true; // continue
                    case 17: // ^Q
                        {
                            if (std::rand() % 50 == 42)
                                winstack.overlays[0] = make_bottom_overlay_custom("I wouldn\'t leave if I were you. DOS is much worse. [Y/n]");
                            else
                                winstack.overlays[0] = make_bottom_overlay_custom("Really quit? [Y/n]");
                            return true; // read next input
                        }
                    case 20: // ^T
                        _mode = PrinterMode::Theme;
                        winstack.overlays[0] = make_bottom_overlay_theme();
                        return true; // continue
                    case 1: // ^A
                        show_about();
                        return true;
                    case 23: // ^W
                        _mode = PrinterMode::Move;
                        winstack.overlays[0] = make_bottom_overlay_move();
                        return true;
                    case 5:
                        {
                            if (std::erase_if(window_id, [&](const auto& idx){
                                if (idx.second == winstack.selector_idx)
                                    {
                                        winstack.pop(idx.second);
                                        return true; // erase
                                    }
                                return false; // keep
                            }) == 0) // window without an id
                                winstack.pop(winstack.selector_idx);
                            return true;
                        }
                    default: // we don't have to print err & handle anything
                        winstack.overlays[0] = make_bottom_overlay();
                        return false; // the ONLY case when we release handle
                    }
                break;
            case PrinterMode::Open:
                switch (input)
                    {
                    case 'e':
                        winstack.push(_ebnf); // EBNF
                        _mode = PrinterMode::Normal;
                        winstack.overlays[0] = make_bottom_overlay();
                        return true;
                    case 'r':
                        winstack.push(_rr_tree); // RR TREE
                        _mode = PrinterMode::Normal;
                        winstack.overlays[0] = make_bottom_overlay();
                        return true;
                    case 'f':
                        winstack.push(_fix); // FIX
                        _mode = PrinterMode::Normal;
                        winstack.overlays[0] = make_bottom_overlay();
                        return true;
                    case 27: // esc, ignore the sequence
                        _mode = PrinterMode::Normal;
                        winstack.overlays[0] = make_bottom_overlay();
                        return true;
                    default:
                        _mode = PrinterMode::Normal;
                        winstack.overlays[0] = make_bottom_overlay_custom(std::basic_string<TChar>("Unknown key: ") + std::basic_string<TChar>(input, 1)); // bad key
                        return true;
                    }
                break;
            case PrinterMode::Theme:
                switch (input)
                    {
                    case 10: // enter
                        _mode = PrinterMode::Normal;
                        _style_name.clear();
                        if (_style_select == PrinterThemes::Panic)
                            winstack.overlays[0] = make_bottom_overlay_custom(std::basic_string<TChar>("No such theme"));
                        else
                            {
                                apply_theme(_style_select);
                                _mode = PrinterMode::Normal;
                                _style_select = PrinterThemes::Panic;
                                winstack.overlays[0] = make_bottom_overlay();
                            }
                        return true;
                    case 27: // esc sequence
                        if (last_char != 27) // no prior
                            return true; // continue - wait for the second key
                        else if (last_char == 27)
                            {
                                // ESC-ESC - exit mode
                                _style_select = PrinterThemes::Panic;
                                _style_name.clear();
                                _mode = PrinterMode::Normal;
                                winstack.overlays[0] = make_bottom_overlay();
                                return true;
                            }
                    case '[': // Arrows
                        {
                            // Escape sequence
                            if (last_char == 27)
                                {
                                    int c3 = terminal.getch();
                                    switch (c3)
                                        {
                                        case 'A':
                                            // Move up
                                            if (_style_select == PrinterThemes::Panic) _style_select = (PrinterThemes)0;
                                            _style_select = (PrinterThemes)(((std::size_t)_style_select + 1) % (std::size_t)PrinterThemes::Panic);
                                            _style_name = printer_theme_names[(std::size_t)_style_select];
                                            winstack.overlays[0] = make_bottom_overlay_theme(true);
                                            return true;
                                        case 'B':
                                            // Move down
                                            _style_select = (PrinterThemes)(((std::size_t)_style_select - 1) % (std::size_t)PrinterThemes::Panic);
                                            _style_name = printer_theme_names[(std::size_t)_style_select];
                                            winstack.overlays[0] = make_bottom_overlay_theme(true);
                                            return true;
                                        }
                                    return true; // don't do anything
                                } // else skip
                        }
                    case 8: // backspace
                        if (_style_name.size() > 0) _style_name.pop_back();
                        winstack.overlays[0] = make_bottom_overlay_theme();
                        return true;
                    case 127: // delete
                        if (_style_name.size() > 0) _style_name.pop_back();
                        winstack.overlays[0] = make_bottom_overlay_theme();
                        return true;
                    default:
                        // Append chars to the string
                        if (input >= 32 && input <= 126)
                            _style_name += input;
                        winstack.overlays[0] = make_bottom_overlay_theme();
                        return true; // continue
                    }
            case PrinterMode::Move:
                switch (input)
                    {
                    case 27: // esc sequence
                        if (last_char != 27) // no prior
                            return true; // continue - wait for the second key
                        else if (last_char == 27)
                            {
                                // ESC-ESC - exit mode
                                _mode = PrinterMode::Normal;
                                winstack.overlays[0] = make_bottom_overlay();
                                return true;
                            }
                    case '[':
                        {
                            if (last_char == 27)
                                {
                                    int c3 = terminal.getch();
                                    if (winstack.selector_idx == -1)
                                        return true;
                                    switch (c3)
                                        {
                                        case 'A':
                                            winstack.stack[winstack.selector_idx]._xy.y() -= 1;
                                            break;
                                        case 'B':
                                            winstack.stack[winstack.selector_idx]._xy.y() += 1;
                                            break;
                                        case 'C':
                                            winstack.stack[winstack.selector_idx]._xy.x() += 1;
                                            break;
                                        case 'D':
                                            winstack.stack[winstack.selector_idx]._xy.x() -= 1;
                                            break;
                                        }
                                    winstack.overlays[0] = make_bottom_overlay_move();
                                    return true;
                                }
                        }
                    case 9:
                        winstack.move_selector_tab(1);
                        winstack.overlays[0] = make_bottom_overlay_move();
                        return true;
                    case 'e':
                        if (winstack.selector_idx == -1)
                            return true;
                        winstack.flags[winstack.selector_idx] ^= (std::size_t)IPWindowFlags::AlwaysActive;
                        winstack.overlays[0] = make_bottom_overlay_move();
                        return true;
                    default:
                        _mode = PrinterMode::Normal;
                        winstack.overlays[0] = make_bottom_overlay_custom(std::basic_string<TChar>("Unknown key: ") + std::basic_string<TChar>(input, 1)); // bad key
                        return true;
                    }
            }
        return true;
    }

    void show_about()
    {
        Widget<TChar> about(WidgetLayout::Vertical, {
                Widget<TChar>(std::basic_string<TChar>("   ____                  ______________"), Colors::Primary),
                Widget<TChar>(std::basic_string<TChar>("  / __/_ _____  ___ ____/ ___/ __/ ___/"), Colors::Primary),
                Widget<TChar>(std::basic_string<TChar>(" _\\ \\/ // / _ \\/ -_) __/ /__/ _// (_ / "), Colors::Primary),
                Widget<TChar>(std::basic_string<TChar>("/___/\\_,_/ .__/\\__/_/  \\___/_/  \\___/  "), Colors::Primary),
                Widget<TChar>(std::basic_string<TChar>("        /_/                            "), Colors::Primary),
                Widget<TChar>(std::basic_string<TChar>("Dynamic shift-reduce parser library"), Colors::Primary, Quad(1,0,1,0)),
                Widget<TChar>(std::basic_string<TChar>("  project homepage : https://github.com/enaix/SuperCFG"), Colors::Secondary, Quad(1,0,1,0)),
                Widget<TChar>(std::basic_string<TChar>("  ui library       : https://github.com/enaix/simply-curse"), Colors::Secondary, Quad(1,0,1,0)),
            }, Colors::None, Quad(), Quad(), &_cur_box_style);

        Widget<TChar> close_btn(std::basic_string<TChar>("<back to debugger>"), Colors::Primary, Quad(1,1,1,1));
        close_btn.set_selectable(true);
        close_btn.on_event = [](Widget<TChar>* self, WindowStack<TChar>* window, const IPEvent& ev,
                                const std::vector<int>& path) -> bool
        {
            if (ev.type == EventType::Select || ev.type == EventType::Click)
                {
                    window->pop(window->selector_idx);
                    return true;
                }
            return false;
        };
        about.add_child(close_btn);
        winstack.push(about);
    }

    Widget<TChar> make_bottom_overlay_move()
    {
        std::basic_string<TChar> toggle_vis = ((winstack.selector_idx != -1 ?
                                                winstack.flags[winstack.selector_idx] & (std::size_t)IPWindowFlags::AlwaysActive :
                                                1) ? "Always Vis" : "Default Vis");
        return Widget<TChar>(WidgetLayout::Horizontal, {
                Widget<TChar>(std::basic_string<TChar>("ARROW"), Colors::Accent3),
                Widget<TChar>(std::basic_string<TChar>("Move Cur Win"), Colors::Primary),
                Widget<TChar>(std::basic_string<TChar>("TAB"), Colors::Accent3),
                Widget<TChar>(std::basic_string<TChar>("Next Win"), Colors::Primary),
                Widget<TChar>(std::basic_string<TChar>("E"), Colors::Accent3),
                Widget<TChar>(std::basic_string<TChar>(toggle_vis), Colors::Primary),
                Widget<TChar>(std::basic_string<TChar>("ESC-ESC"), Colors::Accent3),
                Widget<TChar>(std::basic_string<TChar>("Exit Mode"), Colors::Primary),
            }, Colors::None, Quad(), Quad(1,0,1,0));
    }

    Widget<TChar> make_bottom_overlay_theme(bool autocomplete = false)
    {
        if (_style_name.size() == 0)
            {
                return Widget<TChar>(WidgetLayout::Horizontal, {
                        Widget<TChar>(std::basic_string<TChar>("App theme: "), Colors::Accent),
                        Widget<TChar>(std::basic_string<TChar>("UP/DOWN to choose theme, ESC-ESC to exit"), Colors::Secondary)
                    }, Colors::None);
            }

        if (autocomplete)
            {
                return Widget<TChar>(WidgetLayout::Horizontal, {
                        Widget<TChar>(std::basic_string<TChar>("App theme: "), Colors::Accent),
                        Widget<TChar>(_style_name, Colors::Secondary)
                    }, Colors::None);
            }
        _style_select = PrinterThemes::Panic;
        for (std::size_t i = 0; i < printer_theme_names.size(); i++)
            {
                if (printer_theme_names[i].starts_with(_style_name))
                    {
                        _style_select = (PrinterThemes)i;
                        break;
                    }
            }

        // autocomplete
        std::string postfix;
        if (_style_select != PrinterThemes::Panic)
            {
                const auto start = printer_theme_names[(std::size_t)_style_select].begin() + _style_name.size();
                const auto end = printer_theme_names[(std::size_t)_style_select].end();
                if (start < end)
                    postfix = std::string(start, end);
            }

        return Widget<TChar>(WidgetLayout::Horizontal, {
                Widget<TChar>(std::basic_string<TChar>("App theme: "), Colors::Accent),
                Widget<TChar>(_style_name, Colors::Primary),
                Widget<TChar>(postfix, Colors::Secondary)
            }, Colors::None);
    }

    Widget<TChar> make_bottom_overlay_custom(const std::basic_string<TChar>& text)
    {
        return Widget<TChar>(text, Colors::Secondary);
    }

    void set_bottom_overlay_pos()
    {
        // just move to the bottom of the screen
        winstack.overlays[0]._xy = Point(0, terminal._rows - 1);
    }

    Widget<TChar> make_bottom_overlay()
    {
        switch (_mode)
            {
            case PrinterMode::Normal:
                return Widget<TChar>(WidgetLayout::Horizontal, {
                        Widget<TChar>(std::basic_string<TChar>("TAB"), Colors::Accent3),
                        Widget<TChar>(std::basic_string<TChar>("Next Win"), Colors::Primary),
                        Widget<TChar>(std::basic_string<TChar>("^O"), Colors::Accent3),
                        Widget<TChar>(std::basic_string<TChar>("Open Win"), Colors::Primary),
                        Widget<TChar>(std::basic_string<TChar>("^Q"), Colors::Accent3),
                        Widget<TChar>(std::basic_string<TChar>("Quit"), Colors::Primary),
                        Widget<TChar>(std::basic_string<TChar>("^W"), Colors::Accent3),
                        Widget<TChar>(std::basic_string<TChar>("Move Win"), Colors::Primary),
                        Widget<TChar>(std::basic_string<TChar>("^E"), Colors::Accent3),
                        Widget<TChar>(std::basic_string<TChar>("Destroy Win"), Colors::Primary),
                        Widget<TChar>(std::basic_string<TChar>("^T"), Colors::Accent3),
                        Widget<TChar>(std::basic_string<TChar>("Theme"), Colors::Primary),
                        Widget<TChar>(std::basic_string<TChar>("^A"), Colors::Accent3),
                        Widget<TChar>(std::basic_string<TChar>("About"), Colors::Primary),
                    }, Colors::None, Quad(), Quad(1,0,1,0));
            case PrinterMode::Open:
                return Widget<TChar>(WidgetLayout::Horizontal, {
                        Widget<TChar>(std::basic_string<TChar>("E"), Colors::Accent3),
                        Widget<TChar>(std::basic_string<TChar>("EBNF Repr"), Colors::Primary),
                        Widget<TChar>(std::basic_string<TChar>("R"), Colors::Accent3),
                        Widget<TChar>(std::basic_string<TChar>("Rev Rules"), Colors::Primary),
                        Widget<TChar>(std::basic_string<TChar>("F"), Colors::Accent3),
                        Widget<TChar>(std::basic_string<TChar>("Fix Heur"), Colors::Primary),
                        Widget<TChar>(std::basic_string<TChar>("ESC"), Colors::Accent3),
                        Widget<TChar>(std::basic_string<TChar>("Exit Mode"), Colors::Primary),
                    }, Colors::None, Quad(), Quad(1,0,1,0));
            default:
                break;
            }
        assert((_mode == PrinterMode::Normal || _mode == PrinterMode::Open) && "make_bottom_overlay() can only work in normal and open modes");
        return Widget<TChar>(); // unreachable
    }
};



#endif //PRETTYPRINT_H
