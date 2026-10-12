# TDD roadmap and function objectives

Each increment starts with the named focused test, implements only the stated
contract, then refactors under the full suite. `make test` is the fast feedback
gate; `make sanitize`, `make valgrind`, and `make static-analysis` are the final
refactor gates.

| Increment | Red objective | Green function contract | Refactor checkpoint |
|---|---|---|---|
| 1 | Token/keyword assertions fail | `ssl_token_kind_name` is total; `ssl_keyword_kind` binary-searches exact lowercase keywords | Keep enum order identical to terminal columns |
| 2 | First/duplicate/growth name tests fail | `ssl_name_table_intern` returns stable IDs; `get` bounds-checks; `destroy` releases ownership | Rehash at 70% load with deterministic FNV-1a |
| 3 | Typed constant tests fail | The three add functions append occurrence-specific values; `get` bounds-checks | Share geometric growth and ownership cleanup |
| 4 | Position and primitive token tests fail | Lexer initialization owns input; `ssl_lexer_next` skips ASCII whitespace and returns positioned integer tokens | Centralize byte advancement and lexeme completion |
| 5 | Longest-match/literal tests fail | Lexer recognizes every documented operator and literal and assigns secondary IDs | Keep classification locale-independent |
| 6 | Malformed input tests fail | Lexer reports underscore/byte, incomplete operator, overflow, and unterminated/excess character data at the start position | All failures leave owned pools destructible |
| 7 | Table-invariant test fails | `ssl_parser_validate_tables` rejects out-of-range states, rules, and LHS symbols | Keep runtime tables `static const` and generator-free |
| 8 | Declaration fixtures fail | `ssl_parser_parse` performs shift/reduce/GOTO/accept over productions 1-24 | Use one dynamically growing state stack |
| 9 | Statement fixtures fail | Productions 25-34 accept blocks/control flow; the sole `ELSE` conflict shifts | Keep conflict policy in the static table, not ad hoc runtime code |
| 10 | Expression fixtures fail | Productions 35-69 accept precedence, calls, and lvalues | Preserve O(1) dense-table lookup |
| 11 | Invalid grammar cases lack context | Parser stops immediately and lists all nonzero terminal actions for its current state | Bound diagnostic formatting |
| 12 | CLI integration assertions fail | `main` implements file/stdin, quiet mode, token dump, diagnostics, and exit codes 0/1/2 | Centralize resource cleanup and keep stdout stable |

Current regression coverage is split between `lexer/tests/test_lexer.c`,
`syntax_analyzer/tests/test_parser.c`, and the syntax-analyzer CLI fixtures. New
bug fixes must first add the smallest reproducing test to the relevant component
before changing production code.

## Function-level objective map

Every production function belongs to one increment above:

- Diagnostics: `ssl_diagnostic_set` records a bounded, positioned message.
- Token layer: `ssl_token_kind_name` is total, `ssl_keyword_kind` distinguishes
  reserved words, and `ssl_token_dispose` releases a token lexeme safely.
- Name storage: `ssl_name_table_init`, `ssl_name_table_intern`,
  `ssl_name_table_get`, and `ssl_name_table_destroy` establish, mutate, query,
  and release stable secondary IDs. Private `hash_name` and `resize_slots`
  supply deterministic open addressing; `copy_string` owns stored names.
- Constant storage: `ssl_constant_pool_init`, the three typed `add` functions,
  `ssl_constant_pool_get`, and `ssl_constant_pool_destroy` manage occurrence
  IDs and tagged values. Private `append_constant` provides checked growth.
- Lexer lifecycle: `ssl_lexer_init_text` and `ssl_lexer_init_file` acquire input,
  private `initialize` establishes invariants, and `ssl_lexer_destroy` releases
  ownership. `copy_bytes`, the three ASCII predicates, `peek`, and `advance`
  are exercised through token/position boundary tests.
- Lexing: `ssl_lexer_next` recognizes one principal/secondary token;
  `finish_token` commits its span/lexeme and `pool_failure` normalizes secondary
  storage failures.
- Parser: `ssl_parser_validate_tables` checks static invariants and
  `ssl_parser_parse` performs the LALR loop. Private stack helpers own its
  dynamically growing state stack; `next_token`, `dump_token`, `dump_escaped`,
  and `syntax_error` implement observable token/diagnostic behavior.
- CLI: private `usage` documents invocation and `main` selects input, maps exit
  codes, reports diagnostics, and cleans up every initialized resource.

## Completion gates

- `make test`: unit, grammar, and CLI regression tests.
- `make sanitize`: AddressSanitizer and UndefinedBehaviorSanitizer.
- `make valgrind`: leaks and invalid memory access.
- `make static-analysis`: Cppcheck warning/performance/portability set.
- `make coverage`: LCOV data excluding tests and static parser table data.
- `make check-format`: non-mutating formatting verification.

Verified refactor baseline: 5,258 lexer checks and 44 parser checks pass;
executable logic coverage is 96.8% of lines and 93.8% of branches. Coverage
markers exclude only allocator fault-injection paths and corruption paths made
unreachable by the checked-in `static const` table; sanitizers and Valgrind
remain separate mandatory gates.
