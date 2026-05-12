from typing import Any, Optional, Callable
from collections import OrderedDict
import itertools as it
import logging
import json

from superggd.operators import *
from superggd.base import *
from superggd.parsers.supercfg import SRConfEnum



class StructuredOptim:
    """Structured grammar optimizer class, provides the parameter space and grammar generator"""
    def __init__(self):
        #self.grammar_generator([], 0)
        pass

    def init_args(self, **kwargs):
        pass

    def grammar_generator(self, solution, solution_idx: int) -> Grammar:
        # Common rule definitions
        # =======================

        #params = self.inverse_gene_mapping(solution)

        # All chars except " and '  (for quoted strings)
        chars_q = NTerm("chars_q")  # chars_quoted
        d_chars_q = Define(chars_q, Repeat(Alter(TermsRange("(", "~"), Term(" "), Term("!"), TermsRange("%", "&"), Term("\n"))))

        digit = NTerm("digit")
        d_digit = Define(digit, Alter(TermsRange("0", "9")))
        number = NTerm("number")
        d_number = Define(number, Repeat(digit))

        # Name-like char (minimal symbols, no whitespaces, cannot start with digit)
        name_char_nodigit = NTerm("name_chars_nodigit")  # A single name char (excluding digit)
        d_name_char_nodigit = Define(name_char_nodigit, Alter(TermsRange("A", "Z"), TermsRange("a", "z"), Term("_"), Term("-")))
        name_chars_digit = NTerm("name_chars_digit")  # 1+ name chars (including digit)
        d_name_chars_digit = Define(name_chars_digit, Repeat(Alter(digit, name_char_nodigit)))
        name_chars = NTerm("name_chars")  # rule definition (name_char_nodigit is used as a prefix)
        d_name_chars = Define(name_chars, Concat(name_char_nodigit, Opt(Repeat(name_chars_digit))))

        # Possible grammar options:
        # simple key-value pairs

        #def get_kv_like():
        #    # key + sep + value
        #    kv_pair = NTerm("kv_pair")
        #    d_kv_pair = Define(kv_pair, Concat(name_chars, Term(params["kv_separator"])))
        return Grammar(name_chars, d_chars_q, d_digit, d_number, d_name_char_nodigit, d_name_chars_digit, d_name_chars)


SUPERGGD_MODULE_EXPORT = StructuredOptim()
