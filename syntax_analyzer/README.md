# Parser e análise semântica

Esta pasta contém o parser LALR(1), a árvore sintática, os analisadores de
escopo e de tipos e o programa `sslcheck`.

## Conteúdo principal

- `include/ssl/parser.h`: interface do parser e da árvore sintática;
- `include/ssl/semantic.h`: interface da análise semântica;
- `src/parser.c`: parser e construção da árvore;
- `src/semantic.c`: análise de escopo e análise de tipos;
- `src/main.c`: programa de linha de comando;
- `tests/`: testes e arquivos `.ssl` usados nos testes.

## Compilar e executar

Execute os comandos a partir da pasta principal do projeto:

```sh
make syntax-analyzer
make test
build/sslcheck syntax_analyzer/tests/fixtures/valid/basic.ssl
```

Os arquivos gerados são:

- `build/sslcheck`: executável principal;
- `build/libssl_syntax.a`: biblioteca do parser e da análise semântica.
