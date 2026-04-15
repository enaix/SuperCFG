from typing import Any, Optional
from collections import OrderedDict
import itertools as it

type SubstrMap = OrderedDict[str, tuple[int, list[tuple[int, int]]]]

class LSystem:
    def __init__(self) -> None:
        self._target: Optional[str] = None
        self._mapping_type: str = "lex"
        self._all_substr: bool = False
        self._num_rules: int = 2

    def populate_argparse_group(self, group: Any) -> None:
        group.add_argument("--lsystem", required=True, help="Target l-system string")
        group.add_argument("--mapping-type", choices=["naive", "lex", "seed"], default=self._mapping_type, help="l-system gene mapping algorithm; naive: merge by inclusion count, lex: merge by lex order, seed: merge by lex order and seed")
        group.add_argument("--all-substr", action="store_true", help="Consider all l-system rule candidates, including those which occur only once")
        group.add_argument("--num_rules", type=int, default=self._num_rules, help="Max number of lsystem rules")

    def init_args(self, args: dict[str, Any]) -> None:
        # Note: some arguments may be missing, argparse is not guaranteed to be called
        self._target = args.get("lsystem")
        self._mapping_type = args.get("mapping_type", self._mapping_type)
        self._all_substr = args.get("all_substr", self._all_substr)
        self._num_rules = args.get("num_rules", self._num_rules)

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
        # ...

    def grammar_generator(self, genome):
        pass

    # Gene mapping helpers
    # ====================

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
