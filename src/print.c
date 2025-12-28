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

#include "cast.h"
#include <stdio.h>

// Simple AST printer (recursive, s-expression style)
static void print_ast_type(Type *ty) {
    if (!ty) {
        printf("nil");
        return;
    }
    switch (ty->kind) {
    case TY_VOID: printf("void"); break;
    case TY_BOOL: printf("_Bool"); break;
    case TY_CHAR: printf(ty->is_unsigned ? "unsigned-char" : "char"); break;
    case TY_SHORT: printf(ty->is_unsigned ? "unsigned-short" : "short"); break;
    case TY_INT: printf(ty->is_unsigned ? "unsigned-int" : "int"); break;
    case TY_LONG: printf(ty->is_unsigned ? "unsigned-long" : "long"); break;
    case TY_FLOAT: printf("float"); break;
    case TY_DOUBLE: printf("double"); break;
    case TY_LDOUBLE: printf("long-double"); break;
    case TY_ENUM: printf("enum"); break;
    case TY_PTR: printf("(ptr "); print_ast_type(ty->base); printf(")"); break;
    case TY_ARRAY: printf("(array %d ", ty->array_len); print_ast_type(ty->base); printf(")"); break;
    case TY_FUNC: printf("(fn ...)"); break;
    case TY_STRUCT: printf("struct"); break;
    case TY_UNION: printf("union"); break;
    case TY_VLA: printf("(vla ...)"); break;
    case TY_ERROR: printf("error"); break;
    case TY_BLOCK: printf("block"); break;
    }
}

static void print_ast_node(Node *node, int indent);

static void print_indent_ast(int indent) {
    for (int i = 0; i < indent; i++) printf("  ");
}

static void print_ast_node(Node *node, int indent) {
    if (!node) {
        print_indent_ast(indent);
        printf("nil\n");
        return;
    }

    print_indent_ast(indent);

    static const char *node_names[] = {
        [ND_NULL_EXPR] = "null-expr", [ND_ADD] = "add", [ND_SUB] = "sub",
        [ND_MUL] = "mul", [ND_DIV] = "div", [ND_NEG] = "neg", [ND_MOD] = "mod",
        [ND_BITAND] = "bitand", [ND_BITOR] = "bitor", [ND_BITXOR] = "bitxor",
        [ND_SHL] = "shl", [ND_SHR] = "shr", [ND_EQ] = "eq", [ND_NE] = "ne",
        [ND_LT] = "lt", [ND_LE] = "le", [ND_ASSIGN] = "assign", [ND_COND] = "cond",
        [ND_COMMA] = "comma", [ND_MEMBER] = "member", [ND_ADDR] = "addr",
        [ND_DEREF] = "deref", [ND_NOT] = "not", [ND_BITNOT] = "bitnot",
        [ND_LOGAND] = "logand", [ND_LOGOR] = "logor", [ND_RETURN] = "return",
        [ND_IF] = "if", [ND_FOR] = "for", [ND_DO] = "do", [ND_SWITCH] = "switch",
        [ND_CASE] = "case", [ND_BLOCK] = "block", [ND_GOTO] = "goto",
        [ND_GOTO_EXPR] = "goto-expr", [ND_LABEL] = "label", [ND_LABEL_VAL] = "label-val",
        [ND_FUNCALL] = "call", [ND_EXPR_STMT] = "expr-stmt", [ND_STMT_EXPR] = "stmt-expr",
        [ND_VAR] = "var", [ND_VLA_PTR] = "vla-ptr", [ND_NUM] = "num",
        [ND_CAST] = "cast", [ND_MEMZERO] = "memzero", [ND_ASM] = "asm",
        [ND_CAS] = "cas", [ND_EXCH] = "exch", [ND_FRAME_ADDR] = "frame-addr",
        [ND_BLOCK_LITERAL] = "block-literal", [ND_BLOCK_CALL] = "block-call",
    };

    const char *name = (node->kind <= ND_BLOCK_CALL) ? node_names[node->kind] : "unknown";
    printf("(%s", name ? name : "unknown");

    // Type annotation
    if (node->ty) {
        printf(" : ");
        print_ast_type(node->ty);
    }

    // Special handling for specific node types
    switch (node->kind) {
    case ND_NUM:
        printf(" %lld", (long long)node->val);
        break;
    case ND_VAR:
        if (node->var && node->var->name)
            printf(" %s", node->var->name);
        break;
    case ND_FUNCALL:
        if (node->func_ty && node->func_ty->name)
            printf(" %.*s", node->func_ty->name->len, node->func_ty->name->loc);
        break;
    default:
        break;
    }

    printf(")\n");

    // Print children
    if (node->lhs) {
        print_indent_ast(indent + 1);
        printf("lhs:\n");
        print_ast_node(node->lhs, indent + 2);
    }
    if (node->rhs) {
        print_indent_ast(indent + 1);
        printf("rhs:\n");
        print_ast_node(node->rhs, indent + 2);
    }
    if (node->cond) {
        print_indent_ast(indent + 1);
        printf("cond:\n");
        print_ast_node(node->cond, indent + 2);
    }
    if (node->then) {
        print_indent_ast(indent + 1);
        printf("then:\n");
        print_ast_node(node->then, indent + 2);
    }
    if (node->els) {
        print_indent_ast(indent + 1);
        printf("else:\n");
        print_ast_node(node->els, indent + 2);
    }
    if (node->body) {
        print_indent_ast(indent + 1);
        printf("body:\n");
        for (Node *n = node->body; n; n = n->next)
            print_ast_node(n, indent + 2);
    }
    if (node->args) {
        print_indent_ast(indent + 1);
        printf("args:\n");
        for (Node *n = node->args; n; n = n->next)
            print_ast_node(n, indent + 2);
    }
}

void cc_print_ast(CAST *vm, Obj *prog) {
    (void)vm; // Unused for now
    for (Obj *obj = prog; obj; obj = obj->next) {
        if (obj->is_function) {
            printf("(function %s", obj->name);
            if (obj->ty && obj->ty->return_ty) {
                printf(" : ");
                print_ast_type(obj->ty->return_ty);
            }
            printf(")\n");

            // Print parameters
            if (obj->params) {
                printf("  params:\n");
                for (Obj *p = obj->params; p; p = p->next) {
                    printf("    (%s : ", p->name);
                    print_ast_type(p->ty);
                    printf(")\n");
                }
            }

            // Print body
            if (obj->body) {
                printf("  body:\n");
                print_ast_node(obj->body, 2);
            }
            printf("\n");
        } else {
            printf("(var %s : ", obj->name);
            print_ast_type(obj->ty);
            if (obj->is_static) printf(" static");
            printf(")\n");
        }
    }
}
