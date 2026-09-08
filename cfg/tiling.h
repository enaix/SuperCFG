#ifndef CFG_TILING_H
#define CFG_TILING_H

#include "cfg/preprocess.h"
#include "cfg/preprocess_factories.h"
#include "cfg/context.h"


template<class VStr, class Tree, class RulesSymbol>
class TilingParser
{
public:
    RulesSymbol _rules;

    explicit TilingParser(const RulesSymbol& rules) : _rules(rules) {}

    VStr run(Tree& node, const VStr& input, bool& ok, const std::size_t maxdepth = 1000)
    {
       /*
        * Rules must be defined in the following notation:
        * RulesDef(Define(nterm, Alter(term_lhs, Concat(terms_rhs))),
        *          ...);
        */
        const auto lhs = grammar_to_lhs();  // std::array of VStr
        const auto rhs = grammar_to_rhs();  // ditto
        //for (const auto& l : lhs) std::cout << l << std::endl;
        //for (const auto& r : rhs) std::cout << r << std::endl;
        //std::cout << "=======" << std::endl;

        VStr res, cur = input;
        std::size_t depth = 0;
        while (depth < maxdepth && step(node, cur, lhs, rhs, res)) { /*std::cout << "res=\"" << res << "\"" << std::endl; std::cin.ignore();*/ cur = res; ok = true; depth++; }
        return res;
    }

    bool step(Tree& node, const VStr& input, const auto& lhs, const auto& rhs, VStr& res)
    {
        std::vector<VStr> dp(input.size() + 1, VStr(""));
        std::vector<bool> is_init(input.size() + 1, false);
        is_init[0] = true;
        bool found = false;

        for (std::size_t i = 0; i < input.size(); i++)
        {
            if (!is_init[i]) { /*std::cout << "skip i=" << i << std::endl;*/ continue; }

            for (std::size_t j = 0; j < RulesSymbol::size(); j++)
            {
                if (std::equal(input.begin() + i, input.begin() + i + rhs[j].size(), rhs[j].begin(), rhs[j].end()) && !is_init[i + rhs[j].size()])
                {
                    // rhs is at pos i in input
                    dp[i + rhs[j].size()] = dp[i] + lhs[j];
                    is_init[i + rhs[j].size()] = true;
                    found = true;
                    /*std::cout << "i=" << i << " j=" << j << "; !is_init=" << !is_init[i + rhs[j].size()] << "; eq="
                              << std::equal(input.begin() + i, input.begin() + i + rhs[j].size() + 1, rhs[j].begin(), rhs[j].end())
                              << "; in=" << std::string(input.begin() + i, input.begin() + i + rhs[j].size())
                              << "; rhs=" << std::string(rhs[j].begin(), rhs[j].end())
                              << "; dp[" << i + rhs[j].size() << "]=" << dp[i + rhs[j].size()] << std::endl;*/
                    //Tree cur()
                    //node.add();
                }
            }

            // check if the symbol is not in lhs; then we add it
            if (std::find(lhs.begin(), lhs.end(), VStr(input[i])) == lhs.end() && !is_init[i + 1])
            {
                dp[i + 1] = dp[i] + input[i];
                is_init[i + 1] = true;
                //std::cout << "i=" << i << "; added \"" << dp[i+1] << "\"; at " << i+1 << std::endl;
                found = true;
            }
            res = dp[input.size()];
        }
        return found;
    }

protected:
    auto grammar_to_lhs() const
    {
        return h_type_morph<std::array<VStr, RulesSymbol::size()>, true>([&]<std::size_t i>(const auto& defs){
            const auto& op_alter = std::get<1>(std::get<i>(defs.terms).terms);
            const auto& lhs_op = std::get<0>(op_alter.terms);
            const auto& lhs = lhs_op.name;
            static_assert(is_term<decltype(lhs_op)>(), "Bad tiling parser grammar format: expected Define(..., Alter(lhs, Concat(rhs_i))), found lhs != Term");
            // Here we also count '\0'
            static_assert(std::decay_t<decltype(lhs)>::size() == 2, "Bad tiling parser grammar format: expected Define(..., Alter(lhs, Concat(rhs_i))), found lhs.size() != 1");
            return VStr(lhs);
        }, IntegralWrapper<RulesSymbol::size()>{}, _rules);
    }

    auto grammar_to_rhs() const
    {
        return h_type_morph<std::array<VStr, RulesSymbol::size()>, true>([&]<std::size_t i>(const auto& defs){
            const auto& op_alter = std::get<1>(std::get<i>(defs.terms).terms);
            static_assert(get_operator<decltype(op_alter)>() == OpType::Alter, "Bad tiling parser grammar format: expected Define(..., Alter(lhs, Concat(rhs_i))), found not Alter");
            const auto& concat_s = std::get<1>(op_alter.terms);
            static_assert(get_operator<decltype(concat_s)>() == OpType::Concat, "Bad tiling parser grammar format: expected Define(..., Alter(lhs, Concat(rhs_i))), found not Concat");
            return VStr(concat_to_str(concat_s.terms, std::make_integer_sequence<std::size_t, std::tuple_size_v<std::decay_t<decltype(concat_s.terms)>>>()));
        }, IntegralWrapper<RulesSymbol::size()>{}, _rules);
    }

    template<class... TSymbol, std::size_t... Idx>
    auto concat_to_str(const std::tuple<TSymbol...>& rhs, std::index_sequence<Idx...>) const
    {
        static_assert((is_term<std::tuple_element_t<Idx, std::decay_t<decltype(rhs)>>>() && ...), "Bad tiling parser grammar format: expected Define(..., Alter(lhs, Concat(rhs_i))), found rhs_i != Term");
        return (std::get<Idx>(rhs).name + ...);
    }


    /*bool is_in_terms(TokenV token) const
    {
        // dead code
        return !_rules.each_or_exit([&](const auto& def){
            const auto& op_alter = std::get<1>(def.terms());
            static_assert(get_operator<decltype(op_alter)>() == OpType::Alter, "Bad tiling parser grammar format: expected Define(..., Alter(lhs, Concat(rhs_i))), found not Alter");
            const auto& lhs = std::get<0>(op_alter.terms());
            static_assert(is_term<decltype(lhs)>(), "Bad tiling parser grammar format: expected Define(..., Alter(lhs, Concat(rhs_i))), found lhs != Term");
            static_assert(lhs.name().size() == 1, "Bad tiling parser grammar format: expected Define(..., Alter(lhs, Concat(rhs_i))), found lhs.size() != 1");
            if (lhs.name() == token.value())
                return true; // token is in rules
            return false;
        });
    }*/
};


#endif // CFG_TILING_H
