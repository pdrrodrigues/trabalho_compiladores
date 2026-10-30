CC ?= cc
AR ?= ar
CPPFLAGS := -Ilexer/include -Isyntax_analyzer/include -Icode_generator/include
CFLAGS ?= -std=c17 -O2 -g
WARNINGS := -Wall -Wextra -Wpedantic -Werror -Wconversion -Wshadow
BUILD_DIR := build

LEXER_SOURCES := lexer/src/diagnostic.c lexer/src/token.c \
	lexer/src/pools.c lexer/src/lexer.c
PARSER_SOURCES := syntax_analyzer/src/parser.c syntax_analyzer/src/semantic.c
CODEGEN_SOURCES := code_generator/src/codegen.c
LIB_SOURCES := $(LEXER_SOURCES) $(PARSER_SOURCES) $(CODEGEN_SOURCES)
APP_SOURCES := $(LIB_SOURCES) syntax_analyzer/src/main.c

LEXER_OBJECTS := $(LEXER_SOURCES:%.c=$(BUILD_DIR)/%.o)
PARSER_OBJECTS := $(PARSER_SOURCES:%.c=$(BUILD_DIR)/%.o)
CODEGEN_OBJECTS := $(CODEGEN_SOURCES:%.c=$(BUILD_DIR)/%.o)
APP_OBJECTS := $(APP_SOURCES:%.c=$(BUILD_DIR)/%.o)
LEXER_TEST_OBJECTS := $(LEXER_OBJECTS) \
	$(BUILD_DIR)/lexer/tests/test_lexer.o
PARSER_TEST_OBJECTS := $(LEXER_OBJECTS) $(PARSER_OBJECTS) \
	$(BUILD_DIR)/syntax_analyzer/tests/test_parser.o
SEMANTIC_TEST_OBJECTS := $(LEXER_OBJECTS) $(PARSER_OBJECTS) \
	$(BUILD_DIR)/syntax_analyzer/tests/test_semantic.o
DEPS := $(APP_OBJECTS:.o=.d) \
	$(BUILD_DIR)/lexer/tests/test_lexer.d \
	$(BUILD_DIR)/syntax_analyzer/tests/test_parser.d \
	$(BUILD_DIR)/syntax_analyzer/tests/test_semantic.d

.PHONY: all lexer syntax-analyzer test sanitize valgrind static-analysis \
	coverage check-format clean

all: lexer syntax-analyzer code-generator

lexer: $(BUILD_DIR)/libssl_lexer.a

syntax-analyzer: $(BUILD_DIR)/libssl_syntax.a $(BUILD_DIR)/sslcheck

code-generator: $(BUILD_DIR)/libssl_codegen.a

$(BUILD_DIR)/libssl_lexer.a: $(LEXER_OBJECTS)
	$(AR) rcs $@ $^

$(BUILD_DIR)/libssl_syntax.a: $(PARSER_OBJECTS)
	$(AR) rcs $@ $^

$(BUILD_DIR)/libssl_codegen.a: $(CODEGEN_OBJECTS)
	$(AR) rcs $@ $^

$(BUILD_DIR)/sslcheck: $(APP_OBJECTS)
	$(CC) $(CFLAGS) $(WARNINGS) $^ -o $@

$(BUILD_DIR)/lexer_tests: $(LEXER_TEST_OBJECTS)
	$(CC) $(CFLAGS) $(WARNINGS) $^ -o $@

$(BUILD_DIR)/parser_tests: $(PARSER_TEST_OBJECTS)
	$(CC) $(CFLAGS) $(WARNINGS) $^ -o $@

$(BUILD_DIR)/semantic_tests: $(SEMANTIC_TEST_OBJECTS)
	$(CC) $(CFLAGS) $(WARNINGS) $^ -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) -MMD -MP -c $< -o $@

test: $(BUILD_DIR)/lexer_tests $(BUILD_DIR)/parser_tests \
	$(BUILD_DIR)/semantic_tests $(BUILD_DIR)/sslcheck
	$(BUILD_DIR)/lexer_tests
	$(BUILD_DIR)/parser_tests
	$(BUILD_DIR)/semantic_tests
	sh syntax_analyzer/tests/test_cli.sh $(BUILD_DIR)/sslcheck \
		syntax_analyzer/tests/fixtures/valid/basic.ssl \
		syntax_analyzer/tests/fixtures/invalid/missing_expression.ssl

sanitize:
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c17 -O1 -g $(WARNINGS) \
		-fsanitize=address,undefined -fno-omit-frame-pointer \
		$(LEXER_SOURCES) lexer/tests/test_lexer.c \
		-o $(BUILD_DIR)/lexer_tests_sanitize
	$(CC) $(CPPFLAGS) -std=c17 -O1 -g $(WARNINGS) \
		-fsanitize=address,undefined -fno-omit-frame-pointer \
		$(LIB_SOURCES) syntax_analyzer/tests/test_parser.c \
		-o $(BUILD_DIR)/parser_tests_sanitize
	$(CC) $(CPPFLAGS) -std=c17 -O1 -g $(WARNINGS) \
		-fsanitize=address,undefined -fno-omit-frame-pointer \
		$(LIB_SOURCES) syntax_analyzer/tests/test_semantic.c \
		-o $(BUILD_DIR)/semantic_tests_sanitize
	ASAN_OPTIONS=detect_leaks=1 $(BUILD_DIR)/lexer_tests_sanitize
	ASAN_OPTIONS=detect_leaks=1 $(BUILD_DIR)/parser_tests_sanitize
	ASAN_OPTIONS=detect_leaks=1 $(BUILD_DIR)/semantic_tests_sanitize

valgrind: $(BUILD_DIR)/lexer_tests $(BUILD_DIR)/parser_tests \
	$(BUILD_DIR)/semantic_tests
	valgrind --quiet --leak-check=full --show-leak-kinds=all \
		--errors-for-leak-kinds=all --error-exitcode=1 \
		$(BUILD_DIR)/lexer_tests
	valgrind --quiet --leak-check=full --show-leak-kinds=all \
		--errors-for-leak-kinds=all --error-exitcode=1 \
		$(BUILD_DIR)/parser_tests
	valgrind --quiet --leak-check=full --show-leak-kinds=all \
		--errors-for-leak-kinds=all --error-exitcode=1 \
		$(BUILD_DIR)/semantic_tests

static-analysis:
	cppcheck --enable=warning,performance,portability --error-exitcode=1 \
		--std=c17 $(CPPFLAGS) lexer/src lexer/tests \
		syntax_analyzer/src syntax_analyzer/tests/test_parser.c \
		syntax_analyzer/tests/test_semantic.c code_generator/src

check-format:
	clang-format --dry-run --Werror lexer/include/ssl/*.h lexer/src/*.c \
		lexer/tests/*.c syntax_analyzer/include/ssl/*.h \
		syntax_analyzer/src/*.c syntax_analyzer/tests/*.c \
		code_generator/include/ssl/*.h code_generator/src/*.c

coverage:
	$(RM) -r $(BUILD_DIR)/coverage
	@mkdir -p $(BUILD_DIR)/coverage
	$(CC) $(CPPFLAGS) -std=c17 -O0 -g $(WARNINGS) --coverage \
		$(LEXER_SOURCES) lexer/tests/test_lexer.c \
		-o $(BUILD_DIR)/coverage/lexer_tests
	$(CC) $(CPPFLAGS) -std=c17 -O0 -g $(WARNINGS) --coverage \
		$(LIB_SOURCES) syntax_analyzer/tests/test_parser.c \
		-o $(BUILD_DIR)/coverage/parser_tests
	$(CC) $(CPPFLAGS) -std=c17 -O0 -g $(WARNINGS) --coverage \
		$(LIB_SOURCES) syntax_analyzer/tests/test_semantic.c \
		-o $(BUILD_DIR)/coverage/semantic_tests
	$(CC) $(CPPFLAGS) -std=c17 -O0 -g $(WARNINGS) --coverage \
		$(APP_SOURCES) -o $(BUILD_DIR)/coverage/sslcheck
	cd $(BUILD_DIR)/coverage && ./lexer_tests && ./parser_tests && ./semantic_tests
	sh syntax_analyzer/tests/test_cli.sh $(BUILD_DIR)/coverage/sslcheck \
		syntax_analyzer/tests/fixtures/valid/basic.ssl \
		syntax_analyzer/tests/fixtures/invalid/missing_expression.ssl
	lcov --capture --directory $(BUILD_DIR)/coverage \
		--output-file $(BUILD_DIR)/coverage.info --rc branch_coverage=1
	lcov --remove $(BUILD_DIR)/coverage.info '*/tests/*' \
		--output-file $(BUILD_DIR)/coverage.filtered.info --rc branch_coverage=1
	lcov --summary $(BUILD_DIR)/coverage.filtered.info --rc branch_coverage=1
	sh syntax_analyzer/tests/check_coverage.sh \
		$(BUILD_DIR)/coverage.filtered.info 90

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPS)
