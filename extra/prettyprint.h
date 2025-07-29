//
// Created by Flynn on 08.07.2025.
//

#ifndef PRETTYPRINT_H
#define PRETTYPRINT_H

#include "curse.h"

#include <ctime>
#include <unordered_map>
#include <format>

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
    Descend
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
        winstack.push_overlay(make_bottom_overlay());
        set_bottom_overlay_pos();
        winstack.selector_idx = 0;

        _ebnf = make_ebnf_preview(rules);
        _rr_tree = make_rr_tree(rr_tree);
        _fix = make_empty_fix();
    }


    template<class TMatches, class AllTerms, class NTermsPosPairs, class TermsPosPairs>
    void init_ctx_classes(const TMatches& rules, const AllTerms& all_t, const NTermsPosPairs& nt_pairs, const TermsPosPairs& t_pairs)
    {
        _fix = make_rules_fix(rules, all_t, nt_pairs, t_pairs);
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

    template<class TMatches, class AllTerms, class NTermsPosPairs, class TermsPosPairs>
    Widget<TChar> make_rules_fix(const TMatches& rules, const AllTerms& all_t, const NTermsPosPairs& nt_pairs, const TermsPosPairs& t_pairs)
    {
        // Each of these classes contains an element and its position in a rule

        Widget<TChar> rr_grid(WidgetLayout::Horizontal, {
            Widget<TChar>(WidgetLayout::Vertical, {}, Colors::None), // NTerm
            Widget<TChar>(WidgetLayout::Vertical, {}, Colors::None), // ->
            Widget<TChar>(WidgetLayout::Vertical, {}, Colors::None) // Symbols
        }, Colors::None, Quad(), Quad(1,0,1,0));

        // Initialize the grid with empty cells
        tuple_each(rules, [&](std::size_t i, const auto& rule){
            rr_grid.at(0).add_child(make_nterm(std::get<0>(rule.terms)));
            rr_grid.at(1).add_child(Widget<TChar>(std::basic_string<TChar>(" -> PRE : ")));
            rr_grid.at(2).add_child(Widget<TChar>(std::basic_string<TChar>("; POST : "))); // separator
        });

        auto create_empty_col = [&](std::size_t col){
            for (std::size_t row = 0; row < rr_grid.widgets_num(); row++)
            {
                rr_grid.at(row)._children.insert(rr_grid.at(row)._children.begin() + col, Widget<TChar>());
            }
        };

        int prefix_start = 2, prefix_end = 2, postfix_start = 3;

        auto populate_grid = [&]<bool is_nt>(const auto& symbol_pairs){
            tuple_each_idx(symbol_pairs, [&]<std::size_t i>(const auto& elem){

                const auto& symbol = [&](){
                    if constexpr (is_nt)
                        // fetch from rules
                        return std::get<0>(std::get<i>(rules).terms);
                    else
                        return std::get<i>(all_t);
                }();

                const auto& [related_types, fix_limits] = elem;
                tuple_each(related_types, [&,fix_limits](std::size_t j, const auto& pack){
                    const auto& [rule, fix] = pack;
                    const auto [pre, post_dist] = fix;
                    const auto [max_pre, min_post] = fix_limits; // Get max prefix and min postfix in this rule
                    const auto post = min_post - post_dist; // Convert post from the distance to the end to an id

                    // find grid positions
                    std::size_t prefix_pos = pre + prefix_start, postfix_pos = postfix_start + post;
                    // initialize grid for the prefix
                    for (int l = prefix_end; l <= prefix_pos + 1; l++)
                        create_empty_col(l);

                    prefix_end = prefix_pos + 1; // now this is the end

                    // initialize grid for the postfix
                    for (int l = rr_grid._children.size(); l <= postfix_pos + 1; l++)
                        create_empty_col(l);

                    rr_grid.at(j).at(prefix_pos).refresh(make_symbol(symbol));
                    rr_grid.at(j).at(postfix_pos).refresh(make_symbol(symbol));
                });
            });
        };
        populate_grid.template operator()<true>(nt_pairs);
        populate_grid.template operator()<false>(t_pairs);

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
        return Widget<TChar>(WidgetLayout::Horizontal, {
            Widget<TChar>(std::basic_string<TChar>("ARROW"), Colors::Accent3),
            Widget<TChar>(std::basic_string<TChar>("Move Cur Win"), Colors::Primary),
            Widget<TChar>(std::basic_string<TChar>("TAB"), Colors::Accent3),
            Widget<TChar>(std::basic_string<TChar>("Next Win"), Colors::Primary),
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
