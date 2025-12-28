/*
 CAST: C AST parser + preprocessor

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
*/

#ifndef CAST_H
#define CAST_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <setjmp.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <assert.h>
#include <libgen.h>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <unistd.h>
#include <dlfcn.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*!
 @struct HashEntry
 @abstract Simple key/value bucket used by the project's HashMap.
 @field key Null-terminated string key.
 @field keylen Length of the key in bytes.
 @field val Pointer to the stored value.
*/
typedef struct HashEntry {
    char *key;
    int keylen;
    void *val;
} HashEntry;

/*!
 @struct HashMap
 @abstract Lightweight open-addressing hashmap used for symbol tables,
           macros, and other small maps.
 @field buckets Pointer to an array of HashEntry buckets.
 @field capacity Number of buckets allocated.
 @field used Number of buckets currently in use.
*/
typedef struct HashMap {
    HashEntry *buckets;
    int capacity;
    int used;
} HashMap;

/*!
 @struct File
 @abstract Represents the contents and metadata of a source file.
 @field name Original filename string.
 @field file_no Unique numeric id for the file.
 @field contents File contents as a NUL-terminated buffer.
 @field display_name Optional name emitted by a `#line` directive.
 @field line_delta Line number delta applied for `#line` handling.
*/
typedef struct File {
    char *name;
    int file_no;
    char *contents;

    // For #line directive
    char *display_name;
    int line_delta;
} File;

/*!
 @struct Relocation
 @abstract A relocation record for a global variable initializer that
           references another global symbol.
*/
typedef struct Relocation {
    struct Relocation *next;
    int offset;
    char **label;
    long addend;
} Relocation;

/*!
 @struct StringArray
 @abstract Dynamic array of strings used for include paths and similar
           small lists.
 @field data Pointer to N entries of char* strings.
 @field capacity Allocated capacity of the array.
 @field len Current length (number of strings stored).
*/
typedef struct StringArray {
    char **data;
    int capacity;
    int len;
} StringArray;

/*!
 @struct EnumConstant
 @abstract Represents an enumerator constant within an enum type.
 @field name Name of the enumerator.
 @field value Integer value of the enumerator.
 @field next Pointer to the next enumerator in the linked list.
*/
typedef struct EnumConstant {
    char *name;
    int value;
    struct EnumConstant *next;
} EnumConstant;

/*!
 @struct Hideset
 @abstract Represents a set of macro names that have been hidden to
           prevent recursive macro expansion.
*/
typedef struct Hideset {
    struct Hideset *next;
    char *name;
} Hideset;

/*!
 @enum TokenKind
 @abstract Kinds of lexical tokens produced by the tokenizer and
           used by the preprocessor and parser.
*/
typedef enum {
    TK_IDENT,   // Identifiers
    TK_PUNCT,   // Punctuators
    TK_KEYWORD, // Keywords
    TK_STR,     // String literals
    TK_NUM,     // Numeric literals
    TK_PP_NUM,  // Preprocessing numbers
    TK_EOF,     // End-of-file markers
} TokenKind;

typedef struct Type Type;

/*!
 @struct Token
 @abstract Token produced by the lexer or by macro expansion.
 @field kind Token kind (see TokenKind).
 @field loc Pointer into the source buffer where the token text begins.
 @field len Number of characters in the token text.
 @field str For string literals: pointer to the unescaped contents.
*/
typedef struct Token {
    TokenKind kind;   // Token kind
    struct Token *next;      // Next token
    int64_t val;      // If kind is TK_NUM, its value
    long double fval; // If kind is TK_NUM, its value
    char *loc;        // Token location
    int len;          // Token length
    Type *ty;         // Used if TK_NUM or TK_STR
    char *str;        // String literal contents including terminating '\0'

    File *file;       // Source location
    char *filename;   // Filename
    int line_no;      // Line number
    int col_no;       // Column number (1-based)
    int line_delta;   // Line number
    bool at_bol;      // True if this token is at beginning of line
    bool has_space;   // True if this token follows a space character
    Hideset *hideset; // For macro expansion
    struct Token *origin;    // If this is expanded from a macro, the original token
} Token;

/*!
 @enum TypeKind
 @abstract Kind tag for the `Type` structure describing C types.
*/
typedef enum {
    TY_VOID     = 0,
    TY_BOOL     = 1,
    TY_CHAR     = 2,
    TY_SHORT    = 3,
    TY_INT      = 4,
    TY_LONG     = 5,
    TY_FLOAT    = 6,
    TY_DOUBLE   = 7,
    TY_LDOUBLE  = 8,
    TY_ENUM     = 9,
    TY_PTR      = 10,
    TY_FUNC     = 11,
    TY_ARRAY    = 12,
    TY_VLA      = 13, // variable-length array
    TY_STRUCT   = 14,
    TY_UNION    = 15,
    TY_ERROR    = 16, // error type for recovery
    TY_BLOCK    = 17, // Apple blocks (closures)
} TypeKind;

typedef struct Node Node;
typedef struct Obj Obj;

/*!
 @struct Type
 @abstract Central representation of a C type in the compiler.
 @field kind One of TypeKind indicating the category of this type.
 @field size sizeof() value in bytes.
 @field align Alignment requirement in bytes.
 @field base For pointer/array types: the referenced element/base type.
*/
struct Type {
    TypeKind kind;
    int size;           // sizeof() value
    int align;          // alignment
    bool is_unsigned;   // unsigned or signed
    bool is_atomic;     // true if _Atomic
    bool is_const;      // true if const-qualified
    bool is_volatile;   // true if volatile-qualified
    struct Type *origin;       // for type compatibility check

    // Pointer-to or array-of type.
    struct Type *base;

    // Declaration
    Token *name;
    Token *name_pos;

    // Array
    int array_len;

    // Variable-length array
    Node *vla_len; // # of elements
    Obj *vla_size; // sizeof() value

    // Struct
    struct Member *members;
    bool is_flexible;
    bool is_packed;

    // Enum (tracks enum constants for code generation)
    EnumConstant *enum_constants;

    // Function type
    struct Type *return_ty;
    struct Type *params;
    bool is_variadic;
    struct Type *next;
};

/*!
 @struct Member
 @abstract Member (field) descriptor for struct and union types.
 @field ty Type of the member.
 @field name Token pointing to the member identifier.
 @field offset Byte offset of the member within its aggregate.
*/
typedef struct Member {
    struct Member *next;
    Type *ty;
    Token *tok; // for error message
    Token *name;
    int idx;
    int align;
    int offset;

    // Bitfield
    bool is_bitfield;
    int bit_offset;
    int bit_width;
} Member;

/*!
 @enum NodeKind
 @abstract Kinds of AST nodes produced by the parser.
*/
typedef enum {
    ND_NULL_EXPR = 0, // Do nothing
    ND_ADD       = 1,       // +
    ND_SUB       = 2,       // -
    ND_MUL       = 3,       // *
    ND_DIV       = 4,       // /
    ND_NEG       = 5,       // unary -
    ND_MOD       = 6,       // %
    ND_BITAND    = 7,       // &
    ND_BITOR     = 8,       // |
    ND_BITXOR    = 9,       // ^
    ND_SHL       = 10,      // <<
    ND_SHR       = 11,      // >>
    ND_EQ        = 12,      // ==
    ND_NE        = 13,      // !=
    ND_LT        = 14,      // <
    ND_LE        = 15,      // <=
    ND_ASSIGN    = 16,      // =
    ND_COND      = 17,      // ?:
    ND_COMMA     = 18,      // ,
    ND_MEMBER    = 19,      // . (struct member access)
    ND_ADDR      = 20,      // unary &
    ND_DEREF     = 21,      // unary *
    ND_NOT       = 22,      // !
    ND_BITNOT    = 23,      // ~
    ND_LOGAND    = 24,      // &&
    ND_LOGOR     = 25,      // ||
    ND_RETURN    = 26,      // "return"
    ND_IF        = 27,      // "if"
    ND_FOR       = 28,      // "for" or "while"
    ND_DO        = 29,      // "do"
    ND_SWITCH    = 30,      // "switch"
    ND_CASE      = 31,      // "case"
    ND_BLOCK     = 32,      // { ... }
    ND_GOTO      = 33,      // "goto"
    ND_GOTO_EXPR = 34,      // "goto" labels-as-values
    ND_LABEL     = 35,      // Labeled statement
    ND_LABEL_VAL = 36,      // [GNU] Labels-as-values
    ND_FUNCALL   = 37,      // Function call
    ND_EXPR_STMT = 38,      // Expression statement
    ND_STMT_EXPR = 39,      // Statement expression
    ND_VAR       = 40,      // Variable
    ND_VLA_PTR   = 41,      // VLA designator
    ND_NUM       = 42,      // Integer
    ND_CAST      = 43,      // Type cast
    ND_MEMZERO   = 44,      // Zero-clear a stack variable
    ND_ASM       = 45,      // "asm"
    ND_CAS       = 46,      // Atomic compare-and-swap
    ND_EXCH      = 47,      // Atomic exchange
    ND_FRAME_ADDR = 48,     // __builtin_frame_address(0)
    ND_BLOCK_LITERAL = 49,  // Block literal ^{ ... }
    ND_BLOCK_CALL = 50,     // Block invocation
} NodeKind;

/*!
 @struct Node
 @abstract Represents a node in the parser's abstract syntax tree.
 @field kind Node kind (see NodeKind).
 @field ty Resolved type of this node (after semantic analysis).
 @field lhs Left-hand side child (when applicable).
 @field rhs Right-hand side child (when applicable).
*/
struct Node {
    NodeKind kind; // Node kind
    struct Node *next;    // Next node
    Type *ty;      // Type, e.g. int or pointer to int
    Token *tok;    // Representative token

    struct Node *lhs;     // Left-hand side
    struct Node *rhs;     // Right-hand side

    // "if" or "for" statement
    struct Node *cond;
    struct Node *then;
    struct Node *els;
    struct Node *init;
    struct Node *inc;

    // "break" and "continue" labels
    char *brk_label;
    char *cont_label;

    // Block or statement expression
    struct Node *body;

    // Struct member access
    Member *member;

    // Function call
    Type *func_ty;
    struct Node *args;
    bool pass_by_stack;
    Obj *ret_buffer;

    // Goto or labeled statement, or labels-as-values
    char *label;
    char *unique_label;
    struct Node *goto_next;

    // Switch
    struct Node *case_next;
    struct Node *default_case;

    // Case
    long begin;
    long end;

    // "asm" string literal
    char *asm_str;

    // Atomic compare-and-swap
    struct Node *cas_addr;
    struct Node *cas_old;
    struct Node *cas_new;

    // Atomic op= operators
    Obj *atomic_addr;
    struct Node *atomic_expr;

    // Variable
    Obj *var;

    // Numeric literal
    int64_t val;
    long double fval;

    // Block literal (Apple blocks extension)
    Obj *block_fn;          // Synthetic function for block's body
    Obj **block_captures;   // Array of captured variables
    int num_block_captures; // Number of captured variables
};

/*!
 @struct Obj
 @abstract Represents a C object: either a variable (global/local) or a
           function. The parser uses Obj for symbol and storage tracking.
 @field name Identifier name of the object.
 @field ty Type of the object.
 @field is_local True for local (stack) variables; false for globals.
*/
struct Obj {
    struct Obj *next;
    char *name;    // Variable name
    Type *ty;      // Type
    Token *tok;    // representative token
    bool is_local; // local or global/function
    int align;     // alignment

    // Local variable
    int offset;
    bool is_param; // true if this is a function parameter
    bool is_captured; // true if accessed by a nested function

    // Global variable or function
    bool is_function;
    bool is_definition;
    bool is_static;
    bool is_constexpr;

    // Global variable
    bool is_tentative;
    bool is_tls;
    char *init_data;
    Relocation *rel;
    Node *init_expr;  // For constexpr: AST of initializer expression

    // Function
    bool is_inline;
    Obj *params;
    Node *body;
    Obj *locals;
    Obj *va_area;
    Obj *alloca_bottom;
    int stack_size;

    // Nested function support (GNU C extension)
    struct Obj *parent_fn;    // Enclosing function (NULL if top-level)
    bool is_nested;           // True if defined inside another function
    int nesting_depth;        // 0 = top-level, 1 = one level deep, etc.

    // Block support (Apple blocks extension)
    bool is_block;            // True if this is a block's synthetic function
    Obj **captures;           // Array of captured outer variables
    int num_captures;         // Number of captured variables
    int block_capture_offset; // For captured vars: offset in block descriptor
    bool is_block_var;        // True if declared with __block storage qualifier

    // Static inline function
    bool is_live;
    bool is_root;
    StringArray refs;
};

/*!
 @struct CondIncl
 @abstract Stack entry used to track nested #if/#elif/#else processing
           during preprocessing.
*/
typedef struct CondIncl {
    struct CondIncl *next;
    enum { IN_THEN, IN_ELIF, IN_ELSE } ctx;
    Token *tok;
    bool included;
} CondIncl;

/*!
 @struct VarScopeNode
 @abstract Linked list node for variable/typedef scope entries.
*/
typedef struct VarScopeNode {
    // VarScope fields (must come first for casting)
    Obj *var;
    Type *type_def;
    Type *enum_ty;
    int enum_val;
    // Additional fields for linked list
    char *name;
    int name_len;
    struct VarScopeNode *next;
} VarScopeNode;

/*!
 @struct TagScopeNode
 @abstract Linked list node for struct/union/enum tag scope entries.
*/
typedef struct TagScopeNode {
    char *name;
    int name_len;
    Type *ty;
    struct TagScopeNode *next;
} TagScopeNode;

typedef struct Scope {
    struct Scope *next;
    VarScopeNode *vars;  // Linked list of variables/typedefs
    TagScopeNode *tags;  // Linked list of tags
} Scope;

/*!
 @struct CompileError
 @abstract Represents a compilation error or warning collected during compilation.
*/
typedef struct CompileError {
    struct CompileError *next;  // Next error in linked list
    char *message;              // Formatted error message
    char *filename;             // Source file name
    int line_no;                // Line number
    int col_no;                 // Column number
    int severity;               // 0 = error, 1 = warning
} CompileError;

/*!
 @struct ArenaBlock
 @abstract Represents a single memory block in an arena allocator.
*/
typedef struct ArenaBlock {
    char *base;                 // Start of memory block
    char *ptr;                  // Current allocation pointer (bump pointer)
    size_t size;                // Total block size
    struct ArenaBlock *next;    // Next block in chain
} ArenaBlock;

/*!
 @struct Arena
 @abstract Arena allocator for fast, bulk memory allocation with single deallocation.
*/
typedef struct Arena {
    ArenaBlock *current;        // Current block for allocations
    ArenaBlock *blocks;         // All blocks (for cleanup)
    size_t default_block_size;  // Default block size (1MB)
} Arena;

typedef struct CAST CAST;

/*!
 @struct Compiler
 @abstract Encapsulates all compiler frontend state: preprocessor and parser.
*/
typedef struct Compiler {
    // Preprocessor state
    bool skip_preprocess;               // Skip preprocessing step
    HashMap macros;                     // Macro definitions
    CondIncl *cond_incl;                // Conditional inclusion stack
    HashMap pragma_once;                // #pragma once tracking
    HashMap included_headers;           // Track included headers
    int include_next_idx;               // Index for #include_next

    // #embed directive limits
    size_t embed_limit;                 // Soft limit for #embed size (default: 10MB)
    size_t embed_hard_limit;            // Secondary warning threshold (default: 50MB)
    bool embed_hard_error;              // If true, exceeding limit is a hard error

    bool use_std_inc;                   // Use embedded standard library headers (default: true)

    // Tokenization state
    File *current_file;                 // Input file
    File **input_files;                 // A list of all input files
    bool at_bol;                        // True if at beginning of line
    bool has_space;                     // True if follows a space character

    // Parser state
    Obj *locals;                        // All local variable instances during parsing
    Obj *globals;                       // Global variables accumulated list
    Scope *scope;                       // Current scope
    Obj *initializing_var;              // Variable being initialized
    Obj *current_fn;                    // Function being parsed
    int fn_nesting_depth;               // Current function nesting depth
    Node *gotos;                        // Goto statements in current function
    Node *labels;                       // Labels in current function
    char *brk_label;                    // Current break jump target
    char *cont_label;                   // Current continue jump target
    Node *current_switch;               // Switch statement being parsed
    Obj *builtin_alloca;                // Builtin alloca function
    Obj *builtin_setjmp;                // Builtin setjmp function
    Obj *builtin_longjmp;               // Builtin longjmp function

    // Arena allocator for parser frontend
    Arena parser_arena;                 // Fast bump-pointer allocator

    StringArray include_paths;          // Quote include search paths
    StringArray system_include_paths;   // System header search paths
    HashMap include_cache;              // Cache for search_include_paths
    StringArray file_buffers;           // Track allocated file buffers

    // URL include cache
    char *url_cache_dir;                // Directory for caching downloaded headers
    HashMap url_to_path;                // Maps URLs to cached file paths

    // Label counter for generating unique labels
    int label_counter;

    // Per-instance state
    int unique_name_counter;            // Counter for new_unique_name()
    int counter_macro_value;            // __COUNTER__ macro value
} Compiler;

/*!
 @struct CAST
 @abstract Encapsulates all state for the Cast C parser library.
 @discussion The structure contains frontend state (preprocessor, tokenizer, parser).
             All public API functions accept a `CAST *` as the first parameter.
*/
struct CAST {
    // Compiler state (preprocessor, parser)
    Compiler compiler;

    // Debug output control
    int debug_vm;              // Enable debug output

    // Error handling (setjmp/longjmp for exception-like behavior)
    jmp_buf *error_jmp_buf;            // Jump buffer for error handling
    char *error_message;               // Last error message

    // Error collection (for reporting multiple errors)
    CompileError *errors;              // Linked list of collected errors
    CompileError *errors_tail;         // Tail pointer for O(1) append
    int error_count;                   // Number of errors collected
    int warning_count;                 // Number of warnings collected
    int max_errors;                    // Maximum errors before stopping
    bool collect_errors;               // Enable error collection mode
    bool warnings_as_errors;           // Treat warnings as errors
};

/*!
 @function cc_init
 @abstract Initialize a CAST instance.
 @param vm Pointer to an uninitialized CAST struct to initialize.
 @param flags Reserved for future use (pass 0).
*/
void cc_init(CAST *vm, uint32_t flags);

/*!
 @function cc_destroy
 @abstract Free resources owned by a CAST instance.
 @param vm The CAST instance to destroy.
*/
void cc_destroy(CAST *vm);

/*!
 @function cc_get_error_count
 @abstract Get the number of errors collected during compilation.
 @param vm The CAST instance.
 @result The number of errors collected.
*/
int cc_get_error_count(CAST *vm);

/*!
 @function cc_get_warning_count
 @abstract Get the number of warnings collected during compilation.
 @param vm The CAST instance.
 @result The number of warnings collected.
*/
int cc_get_warning_count(CAST *vm);

/*!
 @function cc_has_errors
 @abstract Check if any errors have been collected.
 @param vm The CAST instance.
 @result True if errors exist, false otherwise.
*/
bool cc_has_errors(CAST *vm);

/*!
 @function cc_clear_errors
 @abstract Clear all collected errors and warnings.
 @param vm The CAST instance.
*/
void cc_clear_errors(CAST *vm);

/*!
 @function cc_print_all_errors
 @abstract Print all collected errors and warnings to stderr.
 @param vm The CAST instance.
*/
void cc_print_all_errors(CAST *vm);

/*!
 @function cc_include
 @abstract Add a directory to the compiler's header search paths.
 @param vm The CAST instance.
 @param path Filesystem path to add to include search.
*/
void cc_include(CAST *vm, const char *path);

/*!
 @function cc_system_include
 @abstract Add a directory to the compiler's system header search paths.
 @param vm The CAST instance.
 @param path Filesystem path to add to system include search.
*/
void cc_system_include(CAST *vm, const char *path);

/*!
 @function cc_define
 @abstract Define or override a preprocessor macro for the given VM.
 @param vm The CAST instance.
 @param name Macro identifier (NUL-terminated).
 @param buf Macro replacement text (NUL-terminated).
*/
void cc_define(CAST *vm, char *name, char *buf);

/*!
 @function cc_undef
 @abstract Remove a preprocessor macro definition from the VM.
 @param vm The CAST instance.
 @param name Macro identifier to remove.
*/
void cc_undef(CAST *vm, char *name);

/*!
 @function cc_preprocess
 @abstract Run the preprocessor on a C source file and return a token stream.
 @param vm The CAST instance.
 @param path Path to the source file to preprocess.
 @return Head of the token stream (linked Token list).
*/
Token *cc_preprocess(CAST *vm, const char *path);

/*!
 @function cc_parse
 @abstract Parse a preprocessed token stream into an AST.
 @param vm The CAST instance.
 @param tok Head of the preprocessed token stream.
 @return Linked list of top-level Obj representing globals and functions.
*/
Obj *cc_parse(CAST *vm, Token *tok);

/*!
 @function cc_parse_expr
 @abstract Parse a single C expression from token stream.
 @param vm The CAST instance.
 @param rest Pointer to receive the remaining tokens after parsing.
 @param tok Head of the token stream to parse.
 @return AST node representing the parsed expression.
*/
Node *cc_parse_expr(CAST *vm, Token **rest, Token *tok);

/*!
 @function cc_parse_assign
 @abstract Parse an assignment expression from token stream.
 @param vm The CAST instance.
 @param rest Pointer to receive the remaining tokens after parsing.
 @param tok Head of the token stream to parse.
 @return AST node representing the parsed assignment expression.
*/
Node *cc_parse_assign(CAST *vm, Token **rest, Token *tok);

/*!
 @function cc_parse_stmt
 @abstract Parse a single C statement from token stream.
 @param vm The CAST instance.
 @param rest Pointer to receive the remaining tokens after parsing.
 @param tok Head of the token stream to parse.
 @return AST node representing the parsed statement.
*/
Node *cc_parse_stmt(CAST *vm, Token **rest, Token *tok);

/*!
 @function cc_parse_compound_stmt
 @abstract Parse a compound statement (block) from token stream.
 @param vm The CAST instance.
 @param rest Pointer to receive the remaining tokens after parsing.
 @param tok Head of the token stream to parse (should be opening brace).
 @return AST node representing the parsed compound statement.
*/
Node *cc_parse_compound_stmt(CAST *vm, Token **rest, Token *tok);
void cc_init_parser(CAST *vm);

/*!
 @function cc_link_progs
 @abstract Link multiple parsed programs (Obj lists) into a single program.
 @param vm The CAST instance.
 @param progs Array of Obj* programs (linked lists from cc_parse).
 @param count Number of programs in the array.
 @return A single merged Obj* linked list containing all objects.
*/
Obj *cc_link_progs(CAST *vm, Obj **progs, int count);

/*!
 @function cc_print_tokens
 @abstract Print a token stream to stdout (useful for debugging).
 @param tok Head of the token stream to print.
*/
void cc_print_tokens(Token *tok);

/*!
 @function cc_print_ast
 @abstract Print the AST in a Lisp-like S-expression format.
 @param vm The CAST instance.
 @param prog The head of the object list (from cc_parse).
*/
void cc_print_ast(CAST *vm, Obj *prog);

/*!
 @function cc_output_json
 @abstract Output C header declarations as JSON for FFI wrapper generation.
 @param f Output file stream (e.g., stdout or file opened with fopen).
 @param prog Head of the parsed AST object list (from cc_parse).
*/
void cc_output_json(FILE *f, Obj *prog);

#ifdef __cplusplus
}
#endif
#endif
