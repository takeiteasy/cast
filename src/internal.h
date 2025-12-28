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

 This file was original part of chibicc by Rui Ueyama (MIT) https://github.com/rui314/chibicc
*/

#pragma once

#include "cast.h"

#ifndef __has_include
#define __has_include(x) 0
#endif

#if __has_include(<stdnoreturn.h>)
#include <stdnoreturn.h>
#else
#define noreturn
#endif

#ifndef __attribute__
#define __attribute__(x)
#endif

void strarray_push(StringArray *arr, char *s);
char *format(char *fmt, ...) __attribute__((format(printf, 1, 2)));
Token *preprocess(CAST *vm, Token *tok);

//
// tokenize.c
//

noreturn void error(char *fmt, ...) __attribute__((format(printf, 1, 2)));
void error_at(CAST *vm, char *loc, char *fmt, ...) __attribute__((format(printf, 3, 4)));
void error_tok(CAST *vm, Token *tok, char *fmt, ...) __attribute__((format(printf, 3, 4)));
bool error_tok_recover(CAST *vm, Token *tok, char *fmt, ...) __attribute__((format(printf, 3, 4)));
void warn_tok(CAST *vm, Token *tok, char *fmt, ...) __attribute__((format(printf, 3, 4)));
bool equal(Token *tok, char *op);
Token *skip(CAST *vm, Token *tok, char *op);
bool consume(CAST *vm, Token **rest, Token *tok, char *str);
void convert_pp_tokens(CAST *vm, Token *tok);
File *new_file(CAST *vm, char *name, int file_no, char *contents);
Token *tokenize_string_literal(CAST *vm, Token *tok, Type *basety);
Token *tokenize(CAST *vm, File *file);
Token *tokenize_file(CAST *vm, char *filename);
Token *tokenize_string(CAST *vm, char *name, char *contents);
unsigned char *read_binary_file(CAST *vm, char *path, size_t *out_size);
void cc_output_preprocessed(FILE *f, Token *tok);

#define unreachable() \
    error("internal error at %s:%d", __FILE__, __LINE__)

//
// preprocess.c
//

char *get_std_header(char *filename);
char *search_include_paths(CAST *vm, char *filename, int filename_len, bool is_system);
void init_macros(CAST *vm);
void define_macro(CAST *vm, char *name, char *buf);
void undef_macro(CAST *vm, char *name);
Token *preprocess(CAST *vm, Token *tok);

//
// parse.c
//

Node *new_cast(CAST *vm, Node *expr, Type *ty);
int64_t const_expr(CAST *vm, Token **rest, Token *tok);
Obj *parse(CAST *vm, Token *tok);

//
// type.c
//

extern Type *ty_void;
extern Type *ty_bool;

extern Type *ty_char;
extern Type *ty_short;
extern Type *ty_int;
extern Type *ty_long;

extern Type *ty_uchar;
extern Type *ty_ushort;
extern Type *ty_uint;
extern Type *ty_ulong;

extern Type *ty_float;
extern Type *ty_double;
extern Type *ty_ldouble;

extern Type *ty_error;

bool is_integer(Type *ty);
bool is_flonum(Type *ty);
bool is_numeric(Type *ty);
bool is_error_type(Type *ty);
bool is_compatible(Type *t1, Type *t2);
Type *copy_type(CAST *vm, Type *ty);
Type *pointer_to(CAST *vm, Type *base);
Type *func_type(CAST *vm, Type *return_ty);
Type *array_of(CAST *vm, Type *base, int size);
Type *vla_of(CAST *vm, Type *base, Node *expr);
Type *enum_type(CAST *vm);
Type *struct_type(CAST *vm);
Type *union_type(CAST *vm);
Type *block_type(CAST *vm, Type *return_ty, Type *params);
void add_type(CAST *vm, Node *node);

//
// unicode.c
//

int encode_utf8(char *buf, uint32_t c);
uint32_t decode_utf8(CAST *vm, char **new_pos, char *p);
bool is_ident1(uint32_t c);
bool is_ident2(uint32_t c);
int display_width(CAST *vm, char *p, int len);

//
// arena.c
//

void arena_init(Arena *arena, size_t default_block_size);
void *arena_alloc(Arena *arena, size_t size);
void arena_reset(Arena *arena);
void arena_destroy(Arena *arena);

//
// hashmap.c
//

void *hashmap_get(HashMap *map, const char *key);
void *hashmap_get2(HashMap *map, const char *key, int keylen);
void hashmap_put(HashMap *map, const char *key, void *val);
void hashmap_put2(HashMap *map, const char *key, int keylen, void *val);
void hashmap_delete(HashMap *map, const char *key);
void hashmap_delete2(HashMap *map, const char *key, int keylen);

// Integer key HashMap functions
void *hashmap_get_int(HashMap *map, long long key);
void hashmap_put_int(HashMap *map, long long key, void *val);
void hashmap_delete_int(HashMap *map, long long key);
void hashmap_test(void);

// HashMap iteration
typedef int (*HashMapIterator)(char *key, int keylen, void *val, void *user_data);

void hashmap_foreach(HashMap *map, HashMapIterator iter, void *user_data);
int hashmap_count_if(HashMap *map, HashMapIterator predicate, void *user_data);
void hashmap_test_iteration(void);

//
// json.c
//

void serialize_type_json(FILE *f, Type *ty, int indent);
void print_indent(FILE *f, int indent);
void print_escaped_string(FILE *f, const char *str);

//
// url_fetch.c
//

bool is_url(const char *filename);
void init_url_cache(CAST *vm);
void clear_url_cache(CAST *vm);
char *fetch_url_to_cache(CAST *vm, const char *url);