<style>
@media print {
  @page { size: A4; margin: 2cm; }
  body { font-size: 11pt; line-height: 1.35; }
  h1 { text-align: center; }
  h2 { break-after: avoid; margin-top: 1.4em; }
  pre { break-inside: avoid; font-size: 10pt; }
  table { break-inside: avoid; }
}
</style>

# Grammar of the Simple Script Language

**Reference:** Section 2.1, “A Linguagem de Trabalho,” from
*Compiladores*, version 1.2.3.

This is the context-free grammar implemented by the LALR(1) syntax analyzer in
this project. Its presentation follows the style used in the reference PDF.

## Notation

- Symbols between single quotes are elementary lexemes recognized by the
  lexical analyzer.
- `ID`, `NUM`, `CHR`, and `STR` are regular tokens supplied by the lexer and
  documented in the [lexical contract](../../lexer/docs/language.md).
- The vertical bar (`|`) separates alternatives.
- Every list in this grammar is non-empty.

## Program and external declarations

A Program (`P`) is formed by a List of External Declarations (`LDE`):

```text
P       → LDE

LDE     → LDE DE
        | DE
```

An External Declaration (`DE`) is a Function Declaration (`DF`) or a Type
Declaration (`DT`):

```text
DE      → DF
        | DT
```

## Types and type declarations

A Type (`T`) is one of the four primitive types or an identifier naming a
previously declared type:

```text
T       → 'integer'
        | 'char'
        | 'boolean'
        | 'string'
        | ID
```

A Type Declaration (`DT`) declares an array, a structure, or a synonym:

```text
DT      → 'type' ID '=' 'array' '[' NUM ']' 'of' T
        | 'type' ID '=' 'struct' '{' DC '}'
        | 'type' ID '=' T

DC      → DC ';' LI ':' T
        | LI ':' T
```

## Functions, parameters, and blocks

A Function Declaration (`DF`) contains a non-empty Parameter List (`LP`), a
return Type (`T`), and a Block (`B`):

```text
DF      → 'function' ID '(' LP ')' ':' T B

LP      → LP ',' ID ':' T
        | ID ':' T
```

A Block contains a non-empty List of Variable Declarations (`LDV`) followed by
a non-empty List of Statements (`LS`):

```text
B       → '{' LDV LS '}'

LDV     → LDV DV
        | DV

LS      → LS S
        | S
```

A Variable Declaration and a List of Identifiers (`LI`) are defined by:

```text
DV      → 'var' LI ':' T ';'

LI      → LI ',' ID
        | ID
```

## Statements

```text
S       → 'if' '(' E ')' S
        | 'if' '(' E ')' S 'else' S
        | 'while' '(' E ')' S
        | 'do' S 'while' '(' E ')' ';'
        | B
        | LV '=' E ';'
        | 'break' ';'
        | 'continue' ';'
```

## Expressions

Logical `&&` and `||` operators have the same precedence and associate to the
left, as specified in the PDF grammar:

```text
E       → E '&&' L
        | E '||' L
        | L
```

Relational expressions are defined by:

```text
L       → L '<' R
        | L '>' R
        | L '<=' R
        | L '>=' R
        | L '==' R
        | L '!=' R
        | R
```

Additive and multiplicative expressions are defined by:

```text
R       → R '+' Y
        | R '-' Y
        | Y

Y       → Y '*' F
        | Y '/' F
        | F
```

The expression factors are:

```text
F       → LV
        | '++' LV
        | '--' LV
        | LV '++'
        | LV '--'
        | '(' E ')'
        | ID '(' LE ')'
        | '-' F
        | '!' F
        | 'true'
        | 'false'
        | CHR
        | STR
        | NUM
```

A function call has a non-empty Expression List (`LE`):

```text
LE      → LE ',' E
        | E
```

An L-value (`LV`) can select structure fields and array elements:

```text
LV      → LV '.' ID
        | LV '[' E ']'
        | ID
```

## Lexical-token source

The nonterminals above consume `ID`, `NUM`, `CHR`, `STR`, reserved words, and
elementary symbols supplied by the lexer component. Their regular definitions
are intentionally kept out of this context-free grammar. See the
[project lexical contract](../../lexer/docs/language.md) and the separate
[right-linear grammar for Figure 2.1](../../lexer/docs/automaton-grammar.md).

## Fidelity notes

The implemented productions follow the grammar printed in section 2.1. In
particular, `DE` has only the `DF` and `DT` alternatives: although the nearby
prose mentions variable declarations, the printed production does not permit
`DV` at program scope. Variable declarations remain mandatory inside a block
through `B → '{' LDV LS '}'`.

Two obvious typesetting inconsistencies are normalized without changing the
grammar: the quoted `'T'` in `DC` is the nonterminal `T`, and the unquoted
`while` in the `do` production is the reserved terminal `'while'`.

The two printed `if` productions create the classic dangling-`else` ambiguity.
The LALR table therefore has one documented shift/reduce conflict and chooses
shift, binding each `else` to the nearest eligible `if`. The productions
themselves are not rewritten; see the [table audit](lalr-table.md).

No empty lists, comments, escape sequences, return statement, or additional
language constructs are introduced.
