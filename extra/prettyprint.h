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
        ANSIColor(ANSIColor::FG::Default, ANSIColor::BG::Default), // Primary (delimeters)
        ANSIColor(ANSIColor::FG::BrightBlack, ANSIColor::BG::BrightWhite), // Secondary (NTerms)
        ANSIColor(ANSIColor::FG::BrightBlue, ANSIColor::BG::BrightWhite), // Accent 1 (Operators)
        ANSIColor(ANSIColor::FG::BrightGreen, ANSIColor::BG::BrightWhite), // Accent 2 (Terms)
        ANSIColor(ANSIColor::FG::BrightBlue, ANSIColor::BG::BrightWhite), // Accent 3
        ANSIColor(ANSIColor::FG::BrightWhite, ANSIColor::BG::BrightRed), // Selected
        ANSIColor(ANSIColor::FG::White, ANSIColor::BG::BrightWhite), // Inactive (Grouping)
        ANSIColor(ANSIColor::FG::White, ANSIColor::BG::BrightBlue), // Disabled
        ANSIColor(ANSIColor::FG::White, ANSIColor::BG::BrightRed), // BorderActive
        ANSIColor(ANSIColor::FG::White, ANSIColor::BG::Blue), // BorderInactive
        ANSIColor(ANSIColor::FG::White, ANSIColor::BG::BrightBlue) // BorderDisabled
    )
    {
        terminal.init_renderer();
    }

    ~PrettyPrinter()
    {
        CurseTerminal<ANSIColor, char>::restore_terminal();
    }

    template<class RRTree, class RulesDef>
    void init_windows(const RRTree& rr_tree, const RulesDef& rules)
    {
        winstack.push(make_ebnf_preview(rules));
        winstack.push(make_rr_tree(rr_tree));
    }

    bool process()
    {
        int term_w = terminal.get_terminal_width();
        int term_h = terminal.get_terminal_height();
        terminal.init_matrix(term_h, term_w);

        winstack.render_all(terminal._output_matrix, terminal._color_matrix, style);
        terminal.render_matrix();
        int c = terminal.getch();
        if (c == 'q') return false;
        if (c == '\t') winstack.move_selector_tab(1);
        return true;
    }


protected:
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
        if constexpr (is_operator<TSymbol>())
        {
            if constexpr (get_operator<TSymbol>() == OpType::Concat || get_operator<TSymbol>() == OpType::Alter)
            {
                Widget<TChar> op(WidgetLayout::Horizontal, {
                    Widget<TChar>(std::basic_string<TChar>("("), Colors::Inactive)
                }, Colors::None);
                tuple_each(s.terms, [&](std::size_t i, const auto& symbol){
                    op.add_child(make_recurse_op(symbol));
                    if (i < std::tuple_size_v<std::decay_t<decltype(s.terms)>> - 1)
                        op.add_child(make_bnf_symbol_multi<get_operator<TSymbol>()>());
                });
                op.add_child(Widget<TChar>(std::basic_string<TChar>(")"), Colors::Inactive));
                return op;
            }
            else if constexpr (get_operator<TSymbol>() == OpType::Optional || get_operator<TSymbol>() == OpType::Repeat || get_operator<TSymbol>() == OpType::Group)
            {
                return Widget<TChar>(WidgetLayout::Horizontal, {
                    Widget<TChar>(std::basic_string<TChar>("("), Colors::Inactive),
                    make_bnf_symbol_left<get_operator<TSymbol>()>(),
                    make_recurse_op(std::get<0>(s.terms)),
                    make_bnf_symbol_right<get_operator<TSymbol>()>(),
                    Widget<TChar>(std::basic_string<TChar>(")"), Colors::Inactive)
                }, Colors::None);
            }
            else if constexpr (get_operator<TSymbol>() == OpType::Except)
            {
                return Widget<TChar>(WidgetLayout::Horizontal, {
                    Widget<TChar>(std::basic_string<TChar>("("), Colors::Inactive),
                    Widget<TChar>(std::basic_string<TChar>("("), Colors::Accent),
                    make_recurse_op(std::get<0>(s.terms)),
                    Widget<TChar>(std::basic_string<TChar>("-"), Colors::Accent),
                    make_recurse_op(std::get<1>(s.terms)),
                    Widget<TChar>(std::basic_string<TChar>(")"), Colors::Accent),
                    Widget<TChar>(std::basic_string<TChar>(")"), Colors::Inactive),
                }, Colors::None);
            }
            else if constexpr (get_operator<TSymbol>() == OpType::End)
            {
                return op(WidgetLayout::Horizontal, {
                    make_recurse_op(std::get<0>(s.terms)),
                    Widget<TChar>(std::basic_string<TChar>(";"), Colors::Inactive)
                }, Colors::None);
            }
            else // Repeat* operators
                return make_bnf_symbol_ext_repeat(s);
        }
        else if constexpr (is_nterm<TSymbol>())
        {
            return make_nterm(s);
        }
        else
        {
            if constexpr (is_term<TSymbol>())
                return make_term(s);
            else
                return make_terms_range(s);
        }
    }

    template<class RRTree>
    static Widget<TChar> make_rr_tree(const RRTree& rr_tree)
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
        }, Colors::None, Quad(), Quad(1,1,1,1), DoubleBoxStyle);
    }

    template<class RulesDef>
    static Widget<TChar> make_ebnf_preview(const RulesDef& rules)
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
        }, Colors::None, Quad(), Quad(1,1,1,1), DoubleBoxStyle);
    }
};



#endif //PRETTYPRINT_H
