# Analisador da Simple Script Language

Este projeto implementa, em C17, as etapas iniciais de um compilador para a
Simple Script Language:

- análise léxica;
- análise sintática;
- análise de escopo;
- análise de tipos.

O programa principal é o `sslcheck`. Ele lê um arquivo-fonte, verifica o
programa e mostra uma mensagem de erro quando encontra algum problema.

## Executar no Dev Container

É necessário ter o Docker e o VS Code com a extensão **Dev Containers**.

1. Abra a pasta do projeto no VS Code.
2. Pressione `Ctrl+Shift+P`.
3. Execute **Dev Containers: Reopen in Container**.
4. Aguarde a criação do container.
5. Abra um terminal no VS Code.

Ao criar o container, o comando `make test` é executado automaticamente. Se
isso não acontecer, execute o comando manualmente no terminal.

## Compilar

No terminal do container, na pasta principal do projeto:

```sh
make
```

O executável será criado em `build/sslcheck`.

Para remover os arquivos gerados:

```sh
make clean
```

## Executar os testes

Para executar todos os testes:

```sh
make test
```

Esse comando testa o lexer, o parser, o analisador de escopo e tipos e a
interface de linha de comando.

Também existem verificações adicionais:

```sh
make sanitize       # AddressSanitizer e UndefinedBehaviorSanitizer
make valgrind       # vazamentos de memória
make static-analysis
make check-format
make coverage
```

## Usar o analisador

Depois de compilar, passe um arquivo `.ssl` para o programa:

```sh
build/sslcheck arquivo.ssl
```

Por padrão, o programa executa a análise léxica, sintática, de escopo e de
tipos.

Opções disponíveis:

```sh
build/sslcheck --syntax-only arquivo.ssl
build/sslcheck --tokens arquivo.ssl
build/sslcheck --tokens --syntax-only arquivo.ssl
build/sslcheck - < arquivo.ssl
```

- `--syntax-only` executa somente a análise sintática.
- `--tokens` mostra os tokens reconhecidos pelo lexer.
- `-` lê o código pela entrada padrão.

Exemplo de arquivo válido:

```ssl
type Index = integer

function compute(value: integer): integer {
    var result: integer;
    result = value + 1;
}
```

Quando o programa é válido, a execução termina sem imprimir mensagem de
erro. Os diagnósticos são enviados para `stderr`.

## Organização do projeto

- `lexer/`: lexer, tokens, constantes e testes léxicos.
- `syntax_analyzer/`: parser, árvore sintática, análise de escopo, análise de
  tipos, programa `sslcheck` e testes.
- `syntax_analyzer/src/semantic.c`: implementação das análises de escopo e de
  tipos.
- `build/`: arquivos gerados pela compilação.

O parser constrói uma árvore sintática. Em seguida, o analisador de escopo
resolve as declarações e usos dos identificadores. Por fim, o analisador de
tipos verifica atribuições, operadores, chamadas de função, campos de structs
e índices de arrays.
