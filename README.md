# CAST

**C** **AST** -- C preprocessor and parser written in C. Based off [chibicc](https://github.com/rui314/chibicc). This version is forked from [jcc](https://github.com/takeiteasy/jcc).

See [here](https://takeiteasy.github.io/cast) for some basic documentation on the API.

## Features

- **Robust Preprocessor**: Full C11/C23 preprocessor support including `#include`, macros, conditional compilation.
- **AST Generation**: Parses C code into an Abstract Syntax Tree (AST).
- **AST Printing**: `-a` or `--ast` flag to dump AST as S-expressions.
- **JSON Output**: Output JSON files containing all function, struct, union, enums, and globals definitions.
  - Useful for generating wrappers for FFI.
  - `./cast --json -o lib.json lib.h`
- **Optional libcurl integration**: Include headers from URL.
  - `#include <https://raw.githubusercontent.com/user/repo/main/header.h>`
  - Build with `make CAST_HAS_CURL=1`
- **Embeddable**: Build as a library and embed into other applications.
  - `#include "cast.h"`
  - `libcast.a` or `libcast.dylib`

## Core C Language Support

### Operators

- Arithmetic: `+`, `-`, `*`, `/`, `%`, unary `-`
- Bitwise: `&`, `|`, `^`, `~`, `<<`, `>>`
- Comparison: `==`, `!=`, `<`, `>`, `<=`, `>=`
- Logical: `&&`, `||`, `!`
- Assignment: `=`, `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`, `<<=`, `>>=`
- Increment/decrement: `++`, `--` (both prefix and postfix)
- Ternary: `? :`
- Comma: `,`

### Control Flow

- `if`, `else`
- `for`, `while`, `do-while`
- `switch`, `case`, `default`
- `goto label`, `goto *expr;`, `&&label`
- `break`, `continue`

### Functions

- Function declarations and definitions
- Function calls
- Recursion
- Parameters and return values
- Function pointers
- Variadic functions

### Data Types

- Basic types: `void`, `char`, `short`, `int`, `long`, `float`, `double`, `_Bool`
- Pointers
- Arrays (fixed-size, multidimensional, VLA)
- Strings
- Structs, Unions, Enums
- Bitfields

### Storage Classes & Qualifiers

- `static`, `extern`
- `const`, `volatile`
- `inline`
- `register`, `restrict`

### Other Features

- `typedef`
- `sizeof`, `_Alignof`
- Compound literals
- Designated initializers
- Cast expressions
- `__attribute__`
- `asm` statements

### Preprocessor

- `#include` (with intelligent search path handling)
  - **Standard library headers** (`<stdio.h>`, `<stdlib.h>`, etc.) are **embedded** in the executable.
    - Disable with `--no-std-inc` if you want to use system headers.
  - **User headers** with `"quotes"` search `-I` paths.
  - **System headers** search `--isystem` paths.
  - URL includes (if enabled).
- `#define`, `#undef`, macro expansion.
- `#ifdef`, `#ifndef`, `#if`, `#elif`, `#else`, `#endif`.
- `#pragma once`.
- `__VA_OPT__` (C23).
- `#embed` (C23 binary inclusion).
  - `#embed "file.bin"`.
  - Supports `limit`, `prefix`, `suffix`, `if_empty`.

## Standard Library Support

CAST has a custom standard library (headers like `stdio.h`, `stdlib.h`) that is **embedded** directly into the library/executable. These contain function declarations that allow you to parse standard C code without needing external compiler headers.

## Building

```bash
make cast        # Build cast executable
make all         # Build cast and libcast.dylib

# Optional: Build with libcurl support for URL header imports
make CAST_HAS_CURL=1
```

## Usage

```bash
# Parse and verify C source (no output means success)
./cast program.c

# Dump AST
./cast -a program.c

# Preprocess only
./cast -E program.c

# Generate JSON symbol data
./cast -j program.c

# With include paths and defines
./cast -I./include -DDEBUG -a main.c
```

## C API Example

You can use `cast` as a library to embed C parsing capabilities into your own tools.

```c
#include "cast.h"
#include <stdio.h>

int main() {
    // 1. Initialize compiler state
    CAST cc;
    // Pass 0 for flags (reserved)
    cc_init(&cc, 0);

    // 2. Configure (Optional)
    // cc.compiler.use_std_inc = false; // Disable embedded headers
    cc_include(&cc, "./include");       // Add user include path
    cc_define(&cc, "DEBUG", "1");       // #define DEBUG 1

    // 3. Preprocess file
    // Returns a linked list of tokens
    Token *tok = cc_preprocess(&cc, "file.c");
    
    // Check for preprocessing errors
    if (!tok || cc_has_errors(&cc)) {
        cc_print_all_errors(&cc);
        cc_destroy(&cc);
        return 1;
    }

    // 4. Parse into AST
    // Returns a linked list of global objects (functions/vars)
    Obj *prog = cc_parse(&cc, tok);
    
    // Check for parsing errors
    if (!prog || cc_has_errors(&cc)) {
        cc_print_all_errors(&cc);
        cc_destroy(&cc);
        return 1;
    }

    // 5. Link (Optional)
    // If you have multiple translation units, you can link them:
    // Obj *progs[] = { prog1, prog2 };
    // Obj *full_prog = cc_link_progs(&cc, progs, 2);
    // For a single file, you can skip this or pass just one:
    Obj *progs[] = { prog };
    Obj *linked_prog = cc_link_progs(&cc, progs, 1);

    // 6. Use the data
    // Dump AST to stdout
    cc_print_ast(&cc, linked_prog);
    
    // Or traverse the AST manually:
    // for (Obj *fn = linked_prog; fn; fn = fn->next) {
    //    if (fn->is_function) { ... }
    // }

    // 7. Cleanup
    cc_destroy(&cc);
    return 0;
}
```

## Testing

```bash
make test
```

## LICENSE
```
CAST

Copyright (C) 2025 George Watson

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
```