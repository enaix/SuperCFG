# Configuration options

## Parser configuration

During parser initialization, configuration enum flags may be passed.

### `SRConfEnum::PrettyPrint`

Enable parser state prettyprinting using the generic TPrinter class. Right now only the TUI debugger is implemented (see [`DEBUGGER.md`](DEBUGGER.md)).

### `SRConfEnum::Lookahead`

Generate the FOLLOW set for the grammar and perform one symbol lookahead during parsing. Behavior may be changed in the future

Prevents the reduction if the next symbol is of the same type. May be useful in cases with repeated elements

### `SRConfEnum::ReducibilityChecker`

Note: HeuristicCtx should be preferred over RC(1)

Generate a RC(1) component which checks if a symbol can be reduced on the next step. For some match `m`, it checks if at least one parent symbol can be reduced up to current stack position.

RC(1) routine keeps track of the current context, which is the current recursion depth of each rule. The major limitation of this algorithm is that it cannot discriminate between rules with equal prefixes.

Note: RC(1) performs additional descent for each related type of the match, which increases the runtime cost.

### `SRConfEnum::HeuristicCtx`

Generate an advanced heuristic context analyzer, which checks fixed prefix and postfix positions for each rule and tries to estimate the current context during runtime. Much more powerful heuristic than RC(1). Right now is still in WIP.

## Lexer configuration

### `LexerConfEnum::Legacy`

A simple lexer which cannot handle duplicate terminals. Cannot handle terminals range. Enabled by default

### `LexerConfEnum::AdvancedLexer`

Lexer with duplicate terminals support, each terminal is uniquely defined as its string and rule. Slightly higher runtime cost, preferred over legacy lexer

### `LexerConfEnum::HandleDuplicates`

Manage duplicate terminals and terminals range. Related types of duplicate terminals are merged into one, while ranges are split into sub-ranges. Imposes significant compile-time overhead

### `LexerConfEnum::HandleDupInRuntime`

Move `LexerConfEnum::HandleDuplicates` initialization to runtime lexer class initialization. Does not affect performance during execution loop
