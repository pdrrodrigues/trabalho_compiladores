#!/bin/sh
set -eu

analyzer=$1
valid=$2
invalid=$3

"$analyzer" "$valid"
"$analyzer" syntax_analyzer/tests/fixtures/valid/semantic.ssl
"$analyzer" --syntax-only "$valid"
if "$analyzer" "$invalid" >build/invalid.stdout 2>build/invalid.stderr; then
    echo "invalid fixture was accepted" >&2
    exit 1
else
    test "$?" -eq 1
fi
test ! -s build/invalid.stdout
grep -q "syntax error: found" build/invalid.stderr
grep -q "expected" build/invalid.stderr

if "$analyzer" syntax_analyzer/tests/fixtures/invalid/undeclared.ssl \
    >build/scope.stdout 2>build/scope.stderr; then
    echo "scope error was accepted" >&2
    exit 1
else
    test "$?" -eq 1
fi
grep -q 'undeclared identifier `missing`' build/scope.stderr
grep -q ':3:9:' build/scope.stderr

if "$analyzer" syntax_analyzer/tests/fixtures/invalid/type_mismatch.ssl \
    >build/type.stdout 2>build/type.stderr; then
    echo "type error was accepted" >&2
    exit 1
else
    test "$?" -eq 1
fi
grep -q 'assignment type mismatch' build/type.stderr
"$analyzer" --syntax-only syntax_analyzer/tests/fixtures/invalid/type_mismatch.ssl
"$analyzer" --tokens --syntax-only "$valid" >build/syntax-tokens.stdout

"$analyzer" --tokens "$valid" >build/tokens.stdout
grep -q 'primary=16(TYPE).*secondary=-1' build/tokens.stdout
grep -q 'primary=48(IDENTIFIER).*secondary=0' build/tokens.stdout
grep -q 'primary=0(EOF).*secondary=-1' build/tokens.stdout

"$analyzer" - <"$valid"

if "$analyzer" >build/usage.stdout 2>build/usage.stderr; then
    echo "missing arguments were accepted" >&2
    exit 1
else
    test "$?" -eq 2
fi
grep -q "usage:" build/usage.stderr

if "$analyzer" --wrong "$valid" >build/wrong.stdout 2>build/wrong.stderr; then
    echo "unknown option was accepted" >&2
    exit 1
else
    test "$?" -eq 2
fi
grep -q "usage:" build/wrong.stderr

if "$analyzer" build/does-not-exist.ssl >build/missing.stdout 2>build/missing.stderr; then
    echo "missing input was accepted" >&2
    exit 1
else
    test "$?" -eq 2
fi
grep -q "cannot open input" build/missing.stderr

if "$analyzer" . >build/directory.stdout 2>build/directory.stderr; then
    echo "directory input was accepted" >&2
    exit 1
else
    test "$?" -eq 2
fi
grep -q "failed to read input" build/directory.stderr
