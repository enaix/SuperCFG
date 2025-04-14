## Feature progress

## Parsers

- [X] Implement shift-reduce parsing (beta)
- [ ] Refactor `GrammarSymbol` class to hold multiple types

## Lexer

- [ ] Move current lexer class to legacy mode
- [ ] Add support for multiple terminals with the same name in the new Lexer

## Global

- [ ] Implement `TermsRange` operator with the new lexer
- [ ] **Analyze how to minimize JIT compilation time with Cling**
- [ ] Add reverse baking operation for constructing a grammar from Bakery, add support for SuperCFG to build itself


## Low priority

- [ ] Finish recursive-descent parser
- [ ] Re-enable optional tokens gluing in lexer