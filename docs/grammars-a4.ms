.\" One-page source for Gramaticas-do-projeto.pdf.
.\" Build: groff -Kutf8 -Tpdf -P-pa4 docs/grammars-a4.ms > Gramaticas-do-projeto.pdf
.pl 29.7c
.po 1c
.ll 19c
.nr HM 0
.nr FM 0
.ps 15
.vs 17p
.ft B
.ce 1
Gramáticas do projeto
.ps 8
.vs 10p
.ft R
.ce 1
Compiladores v1.2.3 — analisador léxico e analisador sintático LALR(1)
.sp 4p
.mk COLTOP
.
.de H
.sp 4p
.ft B
.ps 9
\\$*
.ft CR
.ps 7.6
.vs 9.1p
..
.
.po 1c
.ll 9.15c
.nf
.ft CR
.ps 7.6
.vs 9.1p
.H "1. Gramática regular — lexer (Figura 2.1)"
letter    = 'a'..'z' + 'A'..'Z'
digit     = '0'..'9'
separator = ' ' + '\et' + '\ev' + '\er'
          + '\en' + '\ef'

S   -> separator S
     | letter I
     | digit N
     | '-'
     | '+' P
     | ':' C
     | '<' L1

I   -> letter I | digit I | '_' I | epsilon

N   -> digit N | '.' D | epsilon
D   -> digit D | epsilon

P   -> '=' | epsilon
C   -> '='

L1  -> '=' | '<' L2 | epsilon
L2  -> '<' L3 | epsilon
L3  -> epsilon

LEXEME = identifier + integer + decimal + '-'
       + '+' + '+=' + ':=' + '<' + '<='
       + '<<' + '<<<'

SOURCE = separator* . (LEXEME . separator*)*

epsilon denotes the empty word. Scanning uses longest match.
.H "2. Gramática livre de contexto — parser"
P    -> LDE

LDE  -> LDE DE
      | DE

DE   -> DF
      | DT

T    -> 'integer'
      | 'char'
      | 'boolean'
      | 'string'
      | ID

DT   -> 'type' ID '=' 'array' '[' NUM ']'
        'of' T
      | 'type' ID '=' 'struct' '{' DC '}'
      | 'type' ID '=' T

DC   -> DC ';' LI ':' T
      | LI ':' T

DF   -> 'function' ID '(' LP ')' ':' T B

LP   -> LP ',' ID ':' T
      | ID ':' T

B    -> '{' LDV LS '}'

LDV  -> LDV DV
      | DV

DV   -> 'var' LI ':' T ';'

LI   -> LI ',' ID
      | ID

LS   -> LS S
      | S
.
.rt \n[COLTOP]u
.po 10.85c
.ll 9.15c
.nf
.ft CR
.ps 7.6
.vs 9.1p
.H "2. Gramática livre de contexto — continuação"
S    -> 'if' '(' E ')' S
      | 'if' '(' E ')' S 'else' S
      | 'while' '(' E ')' S
      | 'do' S 'while' '(' E ')' ';'
      | B
      | LV '=' E ';'
      | 'break' ';'
      | 'continue' ';'

E    -> E '&&' L
      | E '||' L
      | L

L    -> L '<' R
      | L '>' R
      | L '<=' R
      | L '>=' R
      | L '==' R
      | L '!=' R
      | R

R    -> R '+' Y
      | R '-' Y
      | Y

Y    -> Y '*' F
      | Y '/' F
      | F

F    -> LV
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

LE   -> LE ',' E
      | E

LV   -> LV '.' ID
      | LV '[' E ']'
      | ID
.H "Notação e decisões LALR(1)"
.ft R
.ps 7.4
.vs 9p
ID, NUM, CHR and STR are tokens supplied by the lexer.
All lists are non-empty. The parser follows the printed
productions of section 2.1: DE has only DF and DT.

The quoted 'T' in DC is normalized as nonterminal T.
The unquoted while in do-while is terminal 'while'.
The sole dangling-else conflict shifts ELSE, associating
it with the nearest eligible if.
.sp 5p
.ft I
.ps 6.8
Source: Compiladoresv1.2.3.pdf, Figure 2.1 and Section 2.1.
.br
Project specifications: lexer/docs/automaton-grammar.md and
syntax_analyzer/docs/grammar.md.
