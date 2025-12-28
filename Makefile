# CAST: C AST parser + preprocessor

SRCS := $(wildcard src/*.c)
CFLAGS := -Wall -O0 -g -std=c99 -Wno-deprecated-declarations -Wno-switch
LDFLAGS :=

# Optional libcurl support for URL-based #include directives
# Enable with: make CAST_HAS_CURL=1
ifdef CAST_HAS_CURL
  ifneq ($(CAST_HAS_CURL),0)
    CFLAGS += -DCAST_HAS_CURL=1
    LIBCURL_CFLAGS := $(shell pkg-config --cflags libcurl 2>/dev/null)
    LIBCURL_LDFLAGS := $(shell pkg-config --libs libcurl 2>/dev/null)

    ifeq ($(LIBCURL_CFLAGS),)
      ifeq ($(shell uname -s),Darwin)
        LIBCURL_CFLAGS := -I/opt/homebrew/opt/curl/include -I/usr/local/opt/curl/include
        LIBCURL_LDFLAGS := -L/opt/homebrew/opt/curl/lib -L/usr/local/opt/curl/lib -lcurl
      else
        LIBCURL_CFLAGS := -I/usr/include -I/usr/local/include
        LIBCURL_LDFLAGS := -L/usr/lib -L/usr/local/lib -lcurl
      endif
    endif

    CFLAGS += $(LIBCURL_CFLAGS)
    LDFLAGS += $(LIBCURL_LDFLAGS)
  endif
endif

ifeq ($(OS),Windows_NT)
	EXE := .exe
	DYLIB := .dll
else
	EXE :=
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Darwin)
		DYLIB := .dylib
	else
		DYLIB := .so
	endif
endif

EXE_OUT := cast$(EXE)
LIB_OUT := libcast$(DYLIB)

# Library sources (exclude main.c)
LIB_SRCS := $(filter-out src/main.c, $(SRCS))

default: $(EXE_OUT)

$(EXE_OUT): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

lib: $(LIB_OUT)

$(LIB_OUT): $(LIB_SRCS)
	$(CC) -fpic -shared $(CFLAGS) -o $@ $^ $(LDFLAGS)

all: $(EXE_OUT) $(LIB_OUT)

test: $(EXE_OUT)
	@echo "int main() { return 0; }" > /tmp/cast_test.c
	./$(EXE_OUT) -j /tmp/cast_test.c > /tmp/cast_test.json
	@echo "JSON output test passed"
	./$(EXE_OUT) -E /tmp/cast_test.c > /tmp/cast_test_pp.c
	@echo "Preprocess test passed"
	./$(EXE_OUT) -a /tmp/cast_test.c > /tmp/cast_test.ast
	@echo "AST output test passed"
	./$(EXE_OUT) /tmp/cast_test.c
	@echo "Parse test passed"
	@rm -f /tmp/cast_test.c /tmp/cast_test.json /tmp/cast_test_pp.c /tmp/cast_test.ast
	@echo "All tests passed!"

docs:
	@headerdoc2html src/cast.h -o docs/; \
	gatherheaderdoc docs/; \
	mv docs/masterTOC.html docs/index.html

clean:
	@$(RM) -f $(EXE_OUT) $(LIB_OUT)

.PHONY: default lib all test clean docs
