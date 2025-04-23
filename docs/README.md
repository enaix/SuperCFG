# Documentation

- [Configuration](docs/CONFIGURATION.md)

## Feature progress

## Lexer

- [ ] Add multi-symbol terminals support

## Global

- [X] Implement `TermsRange` operator with the new lexer
- [X] Verify that ranges intersections work correctly
- [ ] Add unicode (wchar_t) support
- [ ] **Analyze how to minimize JIT compilation time with Cling**
- [ ] Add reverse baking operation for constructing a grammar from Bakery, add support for SuperCFG to build itself

## Low priority

- [ ] Finish recursive-descent parser
- [ ] Re-enable optional tokens gluing in lexer

## Completed

- [X] Implement shift-reduce parsing (beta)
- [X] Refactor `GrammarSymbol` class to hold multiple types
- [X] Move current lexer class to legacy mode
- [X] Add support for multiple terminals with the same name in the new Lexer