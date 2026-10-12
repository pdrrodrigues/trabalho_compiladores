<style>
@media print {
  @page { size: A4; margin: 2cm; }
  body { font-size: 11pt; line-height: 1.35; }
  h1 { text-align: center; }
  h2 { break-after: avoid; margin-top: 1.4em; }
  pre, table { break-inside: avoid; }
  pre { font-size: 10pt; }
}
</style>

# Regular Grammar for the Lexical Automaton

**Reference:** Figure 2.1, “Autômato Finito representando um Analisador
Léxico,” in the lexical-analysis chapter of *Compiladores*, version 1.2.3.

This document gives a right-linear grammar equivalent to the finite automaton
shown in Figure 2.1. It describes exactly the valid lexemes recognized by that
illustrative automaton.

> **Important distinction:** Figure 2.1 is a teaching example presented before
> the Simple Script Language grammar. It is not the complete automaton used by
> this project's lexer. The differences are listed at the end of this document.

## Character classes

Following the notation of the PDF:

```text
letter      = 'a'..'z' + 'A'..'Z'

digit       = '0'..'9'

separator   = ' ' + '\t' + '\v' + '\r' + '\n' + '\f'
```

## Nonterminals and automaton states

Each nonterminal represents one state of the finite automaton:

| Nonterminal | Corresponding state |
|---|---|
| `S` | Initial state |
| `I` | Identifier state |
| `N` | Integer-number state |
| `D` | Decimal-number state |
| `P` | State reached after `'+'` |
| `C` | State reached after `':'` |
| `L1` | State reached after one `'<'` |
| `L2` | State reached after two consecutive `'<'` symbols |
| `L3` | State reached after three consecutive `'<'` symbols |

The empty word `ε` occurs only in accepting states. Therefore, a production
ending in `ε` means that the automaton may finish the current lexeme in that
state.

## Right-linear grammar

The initial state skips separators and selects the family of the next lexeme:

```text
S       → separator S
        | letter I
        | digit N
        | '-'
        | '+' P
        | ':' C
        | '<' L1
```

Identifiers begin with a letter and continue with letters, digits, or `_`:

```text
I       → letter I
        | digit I
        | '_' I
        | ε
```

Numbers begin with a digit. The state following the decimal point is accepting
in Figure 2.1, so the illustrated automaton accepts both `12.` and `12.34`:

```text
N       → digit N
        | '.' D
        | ε

D       → digit D
        | ε
```

The plus state accepts either `+` or `+=`:

```text
P       → '='
        | ε
```

The colon state is not accepting. It must be followed by `=` and therefore
recognizes only `:=`:

```text
C       → '='
```

The less-than path recognizes `<`, `<=`, `<<`, and `<<<`:

```text
L1      → '='
        | '<' L2
        | ε

L2      → '<' L3
        | ε

L3      → ε
```

## Equivalent regular expressions

The language generated above can also be summarized as:

```text
identifier  = letter . ( letter + digit + '_' )*

integer     = digit . digit*

decimal     = digit . digit* . '.' . digit*

minus       = '-'

plus        = '+' . ( '=' + ε )

assignment  = ':' . '='

less-family = '<' + '<=' + '<<' + '<<<'
```

Consequently, the set of token lexemes recognized by Figure 2.1 is:

```text
LEXEME = identifier
       + integer
       + decimal
       + minus
       + plus
       + assignment
       + less-family
```

## Recognition of a source string

The automaton recognizes one lexeme per invocation. Separators are consumed in
the initial state, and the scanner restarts at the initial state after returning
an accepted token. A source string is lexically valid when it can be divided,
using longest match, into repetitions of accepted lexemes with optional
separators:

```text
SOURCE = separator* . ( LEXEME . separator* )*
```

For example:

```text
Valid:    alpha + beta
Valid:    value_2:=10
Valid:    total+=3
Valid:    a<<<b
Invalid:  _name        (an identifier cannot begin with '_')
Invalid:  :            (the colon state is not accepting)
Invalid:  &            (there is no transition for '&')
```

## Relationship to the project lexer

The actual Simple Script Language lexer is specified in
[language.md](language.md). Compared with the illustrative automaton in Figure
2.1, the project language:

- does not allow `_` in identifiers;
- recognizes integer numerals but not decimal numerals;
- does not contain `+=`, `:=`, `<<`, or `<<<` tokens;
- adds the reserved words, punctuation, logical/relational operators, character
  literals, and string literals required by section 2.1;
- uses longest match for its own multi-character operators such as `&&`, `||`,
  `<=`, `>=`, `!=`, `==`, `++`, and `--`.

This separation preserves the automaton exactly as printed without extending
the implemented Simple Script Language with unrelated example tokens.
