from typing import Any, Optional, Callable
from collections import OrderedDict
import itertools as it
import logging
import json

from superggd.operators import *
from superggd.base import *
from superggd.parsers.supercfg import SRConfEnum


logger = logging.getLogger(__name__)

type SubstrMap = OrderedDict[str, tuple[int, list[tuple[int, int]]]]

# Helper functions
# ================

'''
https://github.com/jamfromouterspace/levenshtein/
Jamiel Rahi
GPL 2019
'''
def levenshtein(a, b, ratio=False):
    if a == '' :
        return len(b)
    if b == '' :
        return len(a)

    n = len(a)
    m = len(b)
    # lev = np.zeros((n+1,m+1))
    lev = [[0 for i in range(m+1)] for j in range(n+1)]

    for i in range(0,n+1) :
        lev[i][0] = i
    for i in range(0,m+1) :
        lev[0][i] = i

    for i in range(1,n+1) :
        for j in range(1,m+1) :
            insertion = lev[i-1][j] + 1
            deletion = lev[i][j-1] + 1
            substitution = lev[i-1][j-1] + (1 if a[i-1]!= b[j-1] else 0)
            lev[i][j] = min(insertion,deletion,substitution)

    if ratio :
        return (n+m-lev[n][m])/(n+m)
    else :
        return lev[n][m]


def get_lsystem_edit_dist(rules: dict, axiom: str, maxdepth: int, target: str, ratio: bool) -> float:
    s = axiom
    len_delta = abs(len(target) - len(s))
    edit_dist = levenshtein(s, target)

    for _ in range(maxdepth):
        s = "".join(rules.get(ch, ch) for ch in s)  # expand_lsystem
        new_delta = abs(len(target) - len(s))
        if new_delta > len_delta:
            return edit_dist
        len_delta = new_delta
        edit_dist = float(levenshtein(s, target, ratio))

    return edit_dist


class LSystem:
    def __init__(self) -> None:
        self._target: Optional[str] = None
        self._mapping_type: str = "lex"
        self._all_substr: bool = False
        self._num_rules: int = 2
        self._alpha: float = 5.0
        self._beta: float = 0.0
        self._map_axiom: bool = False

        # Generated
        self._groups: list[tuple[int, SubstrMap]] = []  # rule rhs groups
        self._groups_flat: list[tuple[int, list[str]]] = []  # rule rhs group values (for gene -> str mapping)
        self._symbols: list[str] = []  # unique symbols
        self._axioms: list[str] = []  # possible axioms
        self._gene_groups_idx: list[list[int]] = [] # gene groups idx for each rule
        self._gene_symbols_idx: list[int] = [] # rule (lhs) symbol idx for each rule
        self._gene_axiom_id: int = -1  # axiom idx

        # Exports
        self.pygad_params: dict[str, Any] = {"num_generations": 200, "num_parents_mating": 4, "sol_per_pop": 10, "gene_type": int} # , "mutation_num_genes": 2}
        self.parsers_defaults = {"parser_args": {"supercfg_args": [SRConfEnum.EmptyFlag]}}  # Disable lookahead

    def populate_argparse_group(self, group: Any) -> None:
        group.add_argument("--lsystem", required=True, help="Target l-system string")
        group.add_argument("--mapping-type", choices=["naive", "lex", "seed"], default=self._mapping_type, help="l-system gene mapping algorithm; naive: merge by inclusion count, lex: merge by lex order, seed: merge by lex order and seed")
        group.add_argument("--all-substr", action="store_true", help="Consider all l-system rule candidates, including those which occur only once")
        group.add_argument("--num-rules", type=int, default=self._num_rules, help="Max number of lsystem rules")
        group.add_argument("--alpha", type=float, default=self._alpha, help="Parsed string percentage multiplier [0.0, 1.0] -> [0.0, a]")
        group.add_argument("--beta", type=float, default=self._beta, help="Edit distance multiplier [0.0, 1.0] -> [0.0, b]")
        group.add_argument("--map-axiom", type=bool, default=self._map_axiom, help="Add axiom genes")

    def init_args(self, **kwargs):
        # Note: some arguments may be missing, argparse is not guaranteed to be called
        self._target = kwargs.get("lsystem")
        self._mapping_type = kwargs.get("mapping_type", self._mapping_type)
        self._all_substr = kwargs.get("all_substr", self._all_substr)
        self._num_rules = kwargs.get("num_rules", self._num_rules)
        self._alpha = kwargs.get("alpha", self._alpha)
        self._beta = kwargs.get("beta", self._beta)
        self._map_axiom = kwargs.get("map_axiom", self._map_axiom)
        get_applogger().set_extra_params({"lsystem": self._target, "mapping_type": self._mapping_type, "all_substr": self._all_substr, "num_rules": self._num_rules, "alpha": self._alpha, "beta": self._beta, "map_axiom": self._map_axiom})

    def post_init(self) -> None:
        if self._target is None:
            raise ValueError("No target to optimize, please specify the \"lsystem\" argument")

        # Generate candidates for the mapping
        pr = self._get_substr(self._target)
        if not self._all_substr:
            pr_f: SubstrMap = OrderedDict([(k, v) for k, v in pr.items() if v[0] > 1 and len(k) > 1])  # filter out candidates
        else:
            pr_f = pr
        mut, cbi, pairs = self._mut_strong(pr_f)  # get mutex
        # Add missing keys to cbi
        for s1 in set(pr) - set(cbi):
            cbi[s1] = []

        if self._mapping_type == "naive":
            self._groups = self._gene_mapping_naive(pr_f)
        elif self._mapping_type == "lex":
            self._groups = self._gene_mapping_lex(pr_f)
        elif self._mapping_type == "seed":
            self._groups = self._gene_mapping_seed(pr_f, cbi)
        else:
            raise ValueError(f"No such gene mapping: {self._mapping_type}")

        self._groups_flat = list([(group[0], list(group[1].keys())) for group in self._groups])

        # Generate gene space
        # ===================
        if len(self._groups) == 0 or any([len(x[1]) == 0 for x in self._groups]):
            raise ValueError("One of the gene groups is empty")
        gene0 = range(len(self._groups))  # 0th gene selects the top-level group
        gene1 = range(max(map(lambda x: len(x[1]), self._groups)))  # 1th gene selects the max element in nested group, mapped using modulo
        self.pygad_params["gene_space"] = []

        # Populate gene groups
        for i in range(self._num_rules):
            self.pygad_params["gene_space"] += [gene0, gene1]  # unfortunately, we get duplicates
            self._gene_groups_idx.append([2*i, 2*i+1])  # idx of gene0 and gene1

        # Populate rule symbols (lhs), for now we only consider single-symbol rules
        self._symbols = list(set(self._target))
        gene2 = range(len(self._symbols) + 1)  # We also add an empty rule
        for i in range(self._num_rules):
            self.pygad_params["gene_space"].append(gene2)
            self._gene_symbols_idx.append(len(self.pygad_params["gene_space"]) - 1)

        # Add axiom (consider up to size 2)
        self._axioms = self._symbols + list(it.product(''.join(self._symbols), repeat=2))  # axioms of size 1 are more likely
        if self._map_axiom:  # We store axiom candidates, but do not add these genes
            self.pygad_params["gene_space"].append(range(len(self._axioms)))
        self._gene_axiom_id = len(self.pygad_params["gene_space"]) - 1

        get_applogger().save_artifact("lsystem.json", json.dumps({
            "gene_groups": self._groups,
            "symbols": self._symbols,
            "axioms": self._axioms
        }))

        get_applogger().register_csv_field("match")

        self.pygad_params["num_genes"] = len(self.pygad_params["gene_space"])

        # Gene constraint
        if self._num_rules == 2:
            # TODO add a general case
            self.pygad_params["gene_constraint"] = [
                None,  # Rule 0, gene 0: any group
                None,  # Rule 1, gene 1: any value for group
            ]
            def _check_constraint(sub1: str, sub2: str, cbi: dict[str, list[str]]) -> bool:
                if sub1 <= sub2:
                    return sub2 not in cbi[sub1]  # s2 not in "cannot be in" set -> ok
                return sub1 not in cbi[sub2]

            self.pygad_params["gene_constraint"] += [
                None,  # Rule 1+i, gene 0: any group
                lambda sol, values: [val for val in values if _check_constraint(self._gene_to_substr(sol[0], sol[1]), self._gene_to_substr(sol[2], val), cbi)],  # Rule 1+i, gene 1:
            ]
            # lhs rule constraint
            self.pygad_params["gene_constraint"] += [
                None,  # Rule 1+i, gene 0: any group
                lambda sol, values: [val for val in values if not (val >= len(self._symbols) and sol[3] >= len(self._symbols))],  # Check that there is at least 1 rule
            ]
            # Set the remaining as None
            for i in range(len(self.pygad_params["gene_constraint"]), self.pygad_params["num_genes"]):
                self.pygad_params["gene_constraint"].append(None)

    def grammar_generator(self, solution, solution_idx: int) -> Grammar:
        lhs, rhs, axiom = self._solution_to_grammar(solution)

        # Generate the ruleset
        rhs_to_def = lambda x: NTerm(f"rule_{x}") if x in lhs else Term(x)  # Use terminal if symbol is a constant, else use the nterm
        rules: list[Type[BaseOp]] = []
        for i in range(self._num_rules):
            if lhs[i] == "":
                continue
            rules.append(Define(NTerm(f"rule_{lhs[i]}"), Alter(Term(lhs[i]), Concat(*[rhs_to_def(x) for x in rhs[i]]))))
        # Add axiom rule (if needed)
        if self._map_axiom and axiom not in lhs:
            rules.append(Define(NTerm(f"rule_{axiom}"), Concat(*[rhs_to_def(x) for x in axiom])))
        return Grammar(NTerm(f"rule_{axiom}"), *rules)

    def pre_fn(self, solution, solution_idx: int, grammar: Grammar) -> Optional[float]:
        lhs, rhs, axiom = self._solution_to_grammar(solution)
        rules = dict(zip(lhs, rhs))

        edit_min = None
        for a in self._axioms:
            edit = get_lsystem_edit_dist(rules, a, 7, self._target, True)  # use levenshtein ratio
            if edit_min is None:
                edit_min = edit
            else:
                if edit < edit_min:
                    edit_min = edit

        return edit_min

    def fitness_fn(self, solution, solution_idx: int, grammar: Grammar, run_parser: Callable, pre_fn_result: Optional[float]) -> float:
        ok, ast = run_parser(self._target)
        get_applogger().log_extra(solution_idx, "match", 1 if ok else 0)

        if pre_fn_result is None:
            edit_dist = 0.0
        else:
            edit_dist = pre_fn_result

        if ok:
            logger.info("LSystem::run() : matching solution found")
            # TODO add results logging
        if ast is not None:
            # For now we use average AST depth as the target metric
            depths: list[int] = []
            values: list[str] = []
            ast.each(lambda node, depth, is_leaf: depths.append(depth) if is_leaf else None)
            ast.each(lambda node, depth, is_leaf: values.append(node.value))
            v = ''.join(values)
            if len(v) == 0:
                consumed_perc = 0.0  # set to 0.01 if we multiply
            else:
                consumed_perc = float(len(v)) / float(len(self._target))  # how much % of the string it has consumed, higher - better
            return (-edit_dist)*self._beta + self._alpha * consumed_perc + sum(depths) / float(len(depths))  # E[depths]
        # note: it seems that edit_dist rewards longer rules more, which in turn causes us to diverge
        else:
            return 0.0

    def on_gen(self, ga_instance) -> None:
        solution = ga_instance.best_solution()[0]
        lhs, rhs, axiom = self._solution_to_grammar(solution)
        rules = dict(zip(lhs, rhs))
        print(f"lsystem : best grammar : {rules}")

    # Gene mapping helpers
    # ====================
    def _solution_to_grammar(self, solution) -> tuple[list[str], list[str], str]:
        """Convert a solution to its lsystem grammar components"""
        rhs: list[str] = []
        lhs: list[str] = []

        # Get rule rhs
        for i0, i1 in self._gene_groups_idx:
            rhs.append(self._gene_to_substr(solution[i0], solution[i1]))

        # Get rule lhs
        for i in self._gene_symbols_idx:
            val = solution[i]
            if val >= len(self._symbols):
                lhs.append("")  # skip rule
            else:
                lhs.append(self._symbols[val])

        # Get axiom
        if self._map_axiom:
            axiom = self._axioms[solution[self._gene_axiom_id]]
        else:
            axiom = self._axioms[0]  # return the first axiom
        return lhs, rhs, axiom
    
    def _gene_to_substr(self, gene0: int, gene1: int) -> str:
        """Map the gene value to the rule rhs string"""
        #if self._mapping_type == "lex":
        # TODO test the length-based encoding for lex
        group_size = len(self._groups_flat[gene0][1])
        return self._groups_flat[gene0][1][gene1 % group_size]

    @staticmethod
    def _gene_mapping_naive(pr_f: SubstrMap) -> list[tuple[int, SubstrMap]]:
        """Naive gene mapping: each candidate is grouped by the number of inclusions, sorted by length (primary) and starting pos (secondary)."""
        groups = []
        get_count = lambda x: x[1][0]
        data = sorted(pr_f.items(), key=get_count)  # is stable
        for key, group in it.groupby(data, get_count):
            groups.append((key, OrderedDict(group)))
        return groups

    @staticmethod
    def _gene_mapping_lex(pr_f: SubstrMap) -> list[tuple[int, SubstrMap]]:
        """Lexicographical gene mapping: each candidate is grouped by lex order branch and sorted by length."""
        pr_lex: SubstrMap = OrderedDict(sorted(pr_f.items(), key=lambda x: x[0]))
        groups_lex: list[tuple[int, SubstrMap]] = []
        cur_len = 0
        cur_group = 0

        for k, v in pr_lex.items():
            if cur_len != 0 and len(k) > cur_len:
                groups_lex[-1][1][k] = v
            else:
                # create new group
                groups_lex.append((cur_group, OrderedDict([(k, v)])))
                cur_group += 1
            cur_len = len(k)

        return groups_lex

    @staticmethod
    def _gene_mapping_seed(pr_f: SubstrMap, cbi: dict[str, list[str]]) -> list[tuple[int, SubstrMap]]:
        """Seed-based gene mapping: each candidate is grouped by substr seed and lex"""
        s1_seeds, last_seed = LSystem._seeds_merged(cbi)
        pr_seeds = OrderedDict()

        # Add seed to the substrings
        next_seed = last_seed
        for k, v in pr_f.items():
            if k in s1_seeds:
                cur_seed = s1_seeds[k]
            else:
                cur_seed = next_seed
                next_seed += 1

            pr_seeds[k] = (v[0], cur_seed, v[1])

        # Group by seed
        groups_seed = []
        get_seed = lambda x: x[1][1]
        data = sorted(pr_seeds.items(), key=get_seed)  # is stable
        for key, group in it.groupby(data, get_seed):
            # Sort by lex in the seed group and remove seed from the dict
            group_lex = sorted(map(lambda y: (y[0], (y[1][0], y[1][2])), group), key=lambda x: x[0])
            groups_seed.append((key, OrderedDict(group_lex)))
        # TODO Sort groups by seed
        return groups_seed

    @staticmethod
    def _get_substr(s: str) -> SubstrMap:
        """Get non-overlapping substrings from a string with its value count and inclusion positions."""
        pr: SubstrMap = OrderedDict()
        all_subs: list[str] = [] # should be an ordered set
        for l in range(1, len(s)):
            for start in range(0, len(s) - 1):
                # Get all possible substrings
                sub = s[start:start + l]
                if sub not in all_subs:
                    all_subs.append(sub)

        for sub in all_subs:
            # scan for the positions:
            start = 0
            while start < (len(s) - len(sub) + 1):
                if s[start:start + len(sub)] == sub:
                    if pr.get(sub) is not None:
                        old = pr[sub]
                        pr[sub] = (old[0] + 1, old[1] + [(start, start + len(sub))])
                    else:
                        pr[sub] = (1, [(start, start + len(sub))])
                    start += len(sub) # skip over
                else:
                    start += 1
        return pr

    @staticmethod
    def _mut_strong(pr: SubstrMap) -> tuple[dict[str, list[str]], dict[str, list[str]], list[tuple[str, str]]]:
        """Find strongly mutually exclusive substrings, returns the s2->{s1} and s1->{s2} mappings and the {s1, s2} pairs."""
        mut: dict[str, list[str]] = OrderedDict()  # cannot contain: s2 -> s1
        cbi: dict[str, list[str]] = OrderedDict()  # cannot be in: s1 -> s2
        pairs: list[tuple[str, str]] = []
        for i, (s1, (count1, pos1)) in enumerate(pr.items()):
            for j, (s2, (count2, pos2)) in enumerate(pr.items()):

                # strong condition: # of inclusions must be the same
                if j == i or (not s1 in s2) or len(pos1) == 0 or len(pos1) != len(pos2):
                    continue
                # also skip subs of size 1
                #if count1 == 1 or count2 == 1:
                #    continue

                # Check strong condition
                # We assume that sls is sorted
                for k in range(len(pos2)):
                    if not (pos1[k][0] >= pos2[k][0] and pos1[k][1] <= pos2[k][1]):
                        continue
                pairs.append((s1, s2))
                if mut.get(s2) is not None:
                    mut[s2].append(s1)
                else:
                    mut[s2] = [s1]
                if cbi.get(s1) is not None:
                    cbi[s1].append(s2)
                else:
                    cbi[s1] = [s2]
        return mut, cbi, pairs

    @staticmethod
    def _seeds_merged(cbi: dict[str, list[str]]) -> tuple[dict[str, int], int]:
        """
        Get seeds from mutually exclusive strings, returns the substr->seed mapping and the largest unused seed (not len(seeds)).
        Seeds of {s1}->s2 are merged: all seed branches a->b->c, d->b->c are flattened into one seed, elements a,d are not mutex.
        """
        # Get all seeds
        s1_seeds: dict[str, int] = OrderedDict()

        next_seed = 0
        for s1, s2_set in cbi.items():
            # check if the seed already exists for s1 and {s2}
            cur_seeds = set()
            if s1 in s1_seeds:
                cur_seeds.add(s1_seeds[s1])
            # We still have to check the seeds of {s2} in case if there are overlapping seed groups
            for s2 in s2_set:
                if s2 in s1_seeds:  # s2 may have a larger substring counterpart
                    cur_seeds.add(s1_seeds[s2])

            if len(cur_seeds) == 0:
                # New seed
                s1_seeds[s1] = next_seed
                for s2 in s2_set:
                    s1_seeds[s2] = next_seed
                next_seed += 1
            elif len(cur_seeds) == 1:
                # apply the existing seed
                new_seed = list(cur_seeds)[0]
                s1_seeds[s1] = new_seed
                for s2 in s2_set:
                    s1_seeds[s2] = new_seed
            else:
                # seed groups intersect, need to merge
                print(f"seed groups intersect : {cur_seeds}")
                new_seed = list(cur_seeds)[0]
                # O(n) update
                for k in s1_seeds:
                    if s1_seeds[k] in cur_seeds:
                        s1_seeds[k] = new_seed

                s1_seeds[s1] = new_seed
                for s2 in s2_set:
                    s1_seeds[s2] = new_seed

        # TODO reindex seed ids to be strictly sequential
        return s1_seeds, next_seed



SUPERGGD_MODULE_EXPORT = LSystem()
