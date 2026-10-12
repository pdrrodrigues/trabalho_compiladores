# Simple Script Language contract

This project recognizes the language described in section 2.1 of
`Compiladoresv1.2.3.pdf`. The project lexical rules
are documented here, while the PDF Figure 2.1 automaton has its own equivalent
right-linear grammar in [automaton-grammar.md](automaton-grammar.md). The
syntactic grammar is maintained by the syntax-analyzer component in
[grammar.md](../../syntax_analyzer/docs/grammar.md).

Terminal names match `SslTokenKind`; `EOF` is supplied by the lexer and is not
source text. All lists remain non-empty.

## Lexical contract

- Keywords are the lowercase words `array`, `boolean`, `break`, `char`,
  `continue`, `do`, `else`, `false`, `function`, `if`, `integer`, `of`,
  `string`, `struct`, `true`, `type`, `var`, and `while`.
- An identifier is an ASCII letter followed by zero or more ASCII letters or
  digits. Underscores are not part of this language.
- A numeral is one or more decimal digits and must not exceed `INT64_MAX`.
- A character literal is exactly one raw byte between single quotes. A string
  is zero or more raw bytes terminated by the next double quote. There are no
  escape sequences.
- Whitespace is space, tab, LF, CR, form feed, or vertical tab. Comments are
  not recognized.
- Operators use longest match: `&& || <= >= != == ++ --` take precedence over
  their one-character prefixes.

Identifiers receive stable, zero-based secondary IDs from one compilation's
name table. Every literal occurrence receives a new zero-based index in a
single tagged constant pool. All fixed tokens and EOF use secondary value -1.

## PDF fidelity decisions

| PDF issue | Project interpretation |
|---|---|
| Prose says external declarations include variables, but printed `DE` omits `DV` | Follow printed `DE`; variables are local to blocks |
| `DC` prints quoted `T` | Treat it as nonterminal `T` |
| The `do` rule prints an unquoted `while` | Treat it as keyword `WHILE` |
| Both printed `if` productions create a dangling-else ambiguity | Keep both productions and resolve the sole LALR conflict by shifting `ELSE` |
| Token spelling varies between prose/code | Use the enum and terminal names above |

No optional/empty lists, comments, escapes, AST, return statement, or semantic
checks are added.
