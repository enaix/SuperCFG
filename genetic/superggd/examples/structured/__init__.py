from typing import Any, Optional, Callable
from collections import OrderedDict
import itertools as it
import logging
import json
from dataclasses import dataclass
from enum import StrEnum

from superggd.operators import *
from superggd.base import *
from superggd.parsers.supercfg import SRConfEnum


class GPrimitives(StrEnum):
    """Grammar primitives"""
    SCALAR: "scalar"

    KV: "kv"  # key-value pair
    KEY: "key"
    VALUE: "value"
    ATTRS: "attrs"  # optional attributes (XML-like)

    # Containers
    KV_SEQ: "kv_seq"  # key-value sequence (map, dict, hashtable)
    VALUE_SEQ: "value_seq"  # value sequence (array, list)


class GTerms(StrEnum):
    """Configurable terms inside grammar definitions. This enum is used for setting custom values to styles"""

    # TODO check if we should move this to separate enums

    # KV terms
    KV_START: "kv_start" # prefix
    # (key placeholder)
    KV_SEP_PRE: "kv_sep_pre"  # goes after prefix and before the separator
    # TODO this is unintuitive, need to make the sep handling for different styles better
    KV_SEP: "kv_sep"    # key separator (:, =)
    # (value placeholder)
    KV_END: "kv_end"    # postfix

    # SEQ terms
    SEQ_START: "seq_start"  # prefix
    SEQ_SEP: "seq_sep"  # separator
    SEQ_END: "seq_end"  # postfix, ending


class GTokens(StrEnum):
    """Various grammar tokens"""
    NEWLINE = "newline"
    WS = "ws"  # whitespace
    STRONG_WS = "strong_ws"  # whitespace, cannot be empty

# Search space definitions

SEQ_SEP_TYPE_DEFAULT = {
    "comma": ',',
    "symbolic_any": [',', ';'],
    "newline": GTokens.NEWLINE,
    "strong_ws": GTokens.STRONG_WS
}


SEQ_STYLES_DEFAULT = {
    "empty": {GTerms.SEQ_START: '', GTerms.SEQ_END: ''}  # notice: we do not set the separator
}

KV_STYLES_DEFAULT = {
    # We may want to handle g.kv_has_ending or g.kv_has_attrs in a special way
    # KV_SEP_PRE is kinda broken, what if we have a different SEP
    "header": lambda kv_has_endings, kv_has_attrs: {GTerms.KV_START: "", GTerms.KV_SEP_PRE: "BEGIN"}  # TODO figure out if it's ok or not, set ws param (and move this to a class)
}

@dataclass
class GSearchSpaceKV:
    enable_attrs: bool = True  # enable has_attrs gene. If set to False, then has_attrs=False
    is_kv_tree_like_control: bool = False  # make kv_tree_like a control param separate from kv_has_ending
    # TODO add dot notation toggles

    def register_style(name, style_object: dict[GTerms, str] | Any = None):
        """Register a new style and add it to the search space. If style_object is None, add the style from defaults"""
        pass


class GSearchSpace:
    kv_space: GSearchSpaceKV


@dataclass
class GInstance:
    """Set of control/latent params which describe a particular grammar"""
    # KV params
    kv_has_attrs: bool  # KVs have attrs, this is a separate gene
    kv_tree_like: bool  # KVs can be used in nested expressions, kv_end MUST be set to true. Latent by default (depends on kv_has_ending)
    kv_has_ending: bool    # KVs have a closing token





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

        params = self._solution_to_params(solution)

        # All chars except " and '  (for quoted strings)
        # TODO should add escape chars
        chars_q = NTerm("chars_q")  # chars_quoted
        d_chars_q = Define(chars_q, Repeat(Alter(TermsRange("(", "~"), Term(" "), Term("!"), TermsRange("%", "&"), Term("\n"))))

        digit = NTerm("digit")
        d_digit = Define(digit, Alter(TermsRange("0", "9")))
        number = NTerm("number")
        d_number = Define(number, Repeat(digit))  # Can start with 0

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

    def _get_elem(self, params):


    def _solution_to_params(self, solution) -> dict:
        # Map the genome to grammar parameters
        # NOT IMPLEMENTED
        return {}

SUPERGGD_MODULE_EXPORT = StructuredOptim()
