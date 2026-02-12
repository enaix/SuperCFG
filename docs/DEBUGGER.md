# SuperCFG debugger

## Initialization

In order to analyze the inner state of the parser generator, the debugger may be used. In order to use it, enable the `SRConfEnum::PrettyPrint` flag and pass the `PrettyPrinter` instance to the `make_sr_parser()` and `SRParser::run()` functions.

The example of the UI is provided in `extra/dbg.cpp`, and the `PrettyPrinter` class is defined in `extra/prettyprint.h`. You may run the example using the `superdbg` target.

## How to use the TUI

**TODO describe how to use the debugger**

In order to change the keybinds, pass the keybind index from the `keybinds` map (0 - default) to `PrettyPrinter<KEYBIND_ID>`. You may delegate new keybinds in this map.

## Parser structures troubleshooting

Sometimes the debugger is not enough, and you may need to obtain the exact template instantiations. You may use the following compilation flags in order to inspect the intermediate parser generator structures.

### Prefix/postfix structure

This structure is used in heuristic context analyzer. Each term and nterm is mapped to the list of prefix and postfix positions in rules:

```
# for nterms, same structure for terms
tuple<
  # NTERM/TERM 1
  # Describes in which rules the NTERM/TERM 1 is present (based on RR tree), and at which prefix/postfix positions
  tuple<pair<RULE_i, pair<tuple<IC<pre1>, ...>, tuple<IC<post1>, ...>>>, # RULE, {{prefix1, prefix2, ...}, {postfix1, postfix2, ...}} - can be multiple positions
    pair<RULE_j, pair<tuple<IC<pre1>, ...>, tuple<IC<post1>, ...>>>>,  # <- several rules 

  # NTERM/TERM 2
  tuple<pair<RULE_i, pair<tuple<...>, tuple<...>>>, ...>,  # <- several rules 
  ...
>

# prefix/postfix limits
tuple<
  pair< # NTERM 1
    IC<A>, # max prefix
    IC<B>  # max postfix
  >,
  ...
>
```

The debug logging can be enabled with `-DCMAKE_CXX_FLAGS=-DDBG_PRINT_FIX_POS` and overriden with `-DCMAKE_CXX_FLAGS=-DNO_DBG_PRINT_FIX_POS`. Print the indented template using `python3 format_template_inst.py -k 3 PATH_TO_FILE_WITH_TEMPLATE`
