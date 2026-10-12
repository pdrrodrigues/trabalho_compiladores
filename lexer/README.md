# Lexer

Esta pasta contém o analisador léxico do projeto. Ele lê o código-fonte e
produz tokens para o parser.

## Conteúdo

- `include/ssl/`: interfaces públicas do lexer, tokens, constantes e
  diagnósticos;
- `src/`: implementação do lexer;
- `tests/`: testes do lexer;
- `docs/`: detalhes do contrato léxico e da gramática do lexer.

## Compilar e testar

Execute os comandos a partir da pasta principal do projeto:

```sh
make lexer
make build/lexer_tests
build/lexer_tests
```

A biblioteca gerada é `build/libssl_lexer.a`.
