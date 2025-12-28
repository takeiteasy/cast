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
#include "./internal.h"
#include <getopt.h>

static void usage(const char *argv0, int exit_code) {
    printf("CAST: C AST parser + preprocessor\n");
    printf("https://github.com/takeiteasy/cast\n\n");
    printf("Usage: %s [options] file...\n\n", argv0);
    printf("Options:\n");
    printf("\t-h/--help           Show this message\n");
    printf("\t-I <path>           Add <path> to include search paths\n");
    printf("\t   --isystem <path> Add <path> to system include paths\n");
    printf("\t-D <macro>[=def]    Define a macro\n");
    printf("\t-U <macro>          Undefine a macro\n");
    printf("\t-a/--ast            Dump AST to stdout\n");
    printf("\t-P/--print-tokens   Print preprocessed tokens to stdout\n");
    printf("\t-E/--preprocess     Output preprocessed source code\n");
    printf("\t-j/--json           Output declarations as JSON\n");
    printf("\t-X/--no-preprocess  Disable preprocessing step\n");
    printf("\t-o/--out <file>     Write output to <file>\n");
    printf("\t-v/--verbose        Enable verbose output\n");
    printf("\nPreprocessor Options:\n");
    printf("\t   --embed-limit=SIZE        Set #embed file size warning limit (e.g., 50MB, 100mb, default: 10MB)\n");
    printf("\t   --embed-hard-limit        Make #embed limit a hard error instead of warning\n");
    printf("\nError Handling:\n");
    printf("\t   --max-errors=N            Maximum number of errors before stopping (default: 20)\n");
    printf("\t   --Werror                  Treat warnings as errors\n");
    printf("\nExample:\n");
    printf("\t%s -j header.h\n", argv0);
    printf("\t%s -E -o preprocessed.c source.c\n", argv0);
    printf("\t%s -I ./include -D DEBUG header.h\n", argv0);
    printf("\n");
    exit(exit_code);
}

static char *read_stdin_to_tmp(void) {
#if defined(_WIN32)
    char tmpPath[MAX_PATH + 1];
    char tmpFile[MAX_PATH + 1];
    DWORD len = GetTempPathA(MAX_PATH, tmpPath);
    if (len == 0 || len > MAX_PATH)
        return NULL;
    if (GetTempFileNameA(tmpPath, "cast", 0, tmpFile) == 0)
        return NULL;
    HANDLE h = CreateFileA(tmpFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return NULL;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), stdin)) > 0) {
        DWORD written = 0;
        if (!WriteFile(h, buf, (DWORD)n, &written, NULL) || written != (DWORD)n) {
            CloseHandle(h);
            DeleteFileA(tmpFile);
            return NULL;
        }
    }
    if (ferror(stdin)) {
        CloseHandle(h);
        DeleteFileA(tmpFile);
        return NULL;
    }
    CloseHandle(h);
    return _strdup(tmpFile);
#else
    char template[] = "/tmp/cast-stdin-XXXXXX";
    int fd = mkstemp(template);
    if (fd < 0)
        return NULL;
    char buf[4096];
    ssize_t n;
    while ((n = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
        ssize_t w = 0;
        while (w < n) {
            ssize_t m = write(fd, buf + w, n - w);
            if (m < 0) { close(fd); unlink(template); return NULL; }
            w += m;
        }
    }
    if (n < 0) {
        close(fd);
        unlink(template);
        return NULL;
    }
    if (close(fd) < 0) {
        unlink(template);
        return NULL;
    }
    return strdup(template);
#endif
}

static void parse_define(CAST *cc, char *arg) {
    char *eq = strchr(arg, '=');
    if (eq) {
        *eq = '\0';
        cc_define(cc, arg, eq + 1);
    } else
        cc_define(cc, arg, "1");
}

static size_t parse_size(const char *str, const char *flag_name) {
    char *endptr;
    double value = strtod(str, &endptr);

    if (value < 0) {
        fprintf(stderr, "error: %s must be non-negative\n", flag_name);
        exit(1);
    }

    size_t multiplier = 1;
    if (*endptr != '\0') {
        if (strcasecmp(endptr, "kb") == 0 || strcasecmp(endptr, "k") == 0) {
            multiplier = 1024;
        } else if (strcasecmp(endptr, "mb") == 0 || strcasecmp(endptr, "m") == 0) {
            multiplier = 1024 * 1024;
        } else if (strcasecmp(endptr, "gb") == 0 || strcasecmp(endptr, "g") == 0) {
            multiplier = 1024 * 1024 * 1024;
        } else if (strcasecmp(endptr, "b") == 0) {
            multiplier = 1;
        } else {
            fprintf(stderr, "error: invalid size suffix '%s' for %s (use KB, MB, GB, or B)\n", endptr, flag_name);
            exit(1);
        }
    }

    return (size_t)(value * multiplier);
}

int main(int argc, const char* argv[]) {
    int exit_code = 0;
    const char **input_files = NULL;
    int input_files_count = 0;
    const char **inc_paths = NULL;
    int inc_paths_count = 0;
    const char **sys_inc_paths = NULL;
    int sys_inc_paths_count = 0;
    const char **defines = NULL;
    int defines_count = 0;
    const char **undefs = NULL;
    int undefs_count = 0;
    char *out_file = NULL;
    int dump_ast = 0;
    int verbose = 0;
    int print_tokens = 0;
    int preprocess_only = 0;
    int skip_preprocess = 0;
    int output_json = 0;
    int max_errors = 20;
    int warnings_as_errors = 0;
    size_t embed_limit = 0;
    int embed_hard_error = 0;

    if (argc <= 1)
        usage(argv[0], 1);

    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"out", required_argument, 0, 'o'},
        {"verbose", no_argument, 0, 'v'},
        {"ast", no_argument, 0, 'a'},
        {"print-tokens", no_argument, 0, 'P'},
        {"preprocess", no_argument, 0, 'E'},
        {"no-preprocess", no_argument, 0, 'X'},
        {"json", no_argument, 0, 'j'},
        {"include", required_argument, 0, 'I'},
        {"isystem", required_argument, 0, 1001},
        {"define", required_argument, 0, 'D'},
        {"undef", required_argument, 0, 'U'},
        {"max-errors", required_argument, 0, 1002},
        {"Werror", no_argument, 0, 1003},
        {"embed-limit", required_argument, 0, 1004},
        {"embed-hard-limit", no_argument, 0, 1005},
        {0, 0, 0, 0}
    };

    const char *optstring = "haI:D:U:o:vPEXj";
    int opt;
    opterr = 0;
    while ((opt = getopt_long(argc, (char * const *)argv, optstring, long_options, NULL)) != -1) {
        switch (opt) {
        case 'h':
            usage(argv[0], 0);
            break;
        case 'o':
            if (out_file) { fprintf(stderr, "error: only one -o/--out allowed\n"); usage(argv[0], 1); }
            out_file = strdup(optarg);
            break;
        case 'v':
            verbose = 1;
            break;
        case 'a':
            dump_ast = 1;
            break;
        case 'I':
            inc_paths = realloc(inc_paths, sizeof(*inc_paths) * (inc_paths_count + 1));
            inc_paths[inc_paths_count++] = strdup(optarg);
            break;
        case 1001: // --isystem
            sys_inc_paths = realloc(sys_inc_paths, sizeof(*sys_inc_paths) * (sys_inc_paths_count + 1));
            sys_inc_paths[sys_inc_paths_count++] = strdup(optarg);
            break;
        case 'D':
            defines = realloc(defines, sizeof(*defines) * (defines_count + 1));
            defines[defines_count++] = strdup(optarg);
            break;
        case 'U':
            undefs = realloc(undefs, sizeof(*undefs) * (undefs_count + 1));
            undefs[undefs_count++] = strdup(optarg);
            break;
        case 'P':
            print_tokens = 1;
            break;
        case 'E':
            preprocess_only = 1;
            break;
        case 'X':
            skip_preprocess = 1;
            break;
        case 'j':
            output_json = 1;
            break;
        case 1002: // --max-errors
            max_errors = atoi(optarg);
            if (max_errors <= 0) {
                fprintf(stderr, "error: --max-errors must be a positive integer\n");
                usage(argv[0], 1);
            }
            break;
        case 1003: // --Werror
            warnings_as_errors = 1;
            break;
        case 1004: // --embed-limit
            embed_limit = parse_size(optarg, "--embed-limit");
            break;
        case 1005: // --embed-hard-limit
            embed_hard_error = 1;
            break;
        case '?':
            if (optopt)
                fprintf(stderr, "error: option -%c requires an argument\n", optopt);
            else if (optind > 0 && argv[optind-1] && argv[optind-1][0] == '-')
                fprintf(stderr, "error: unknown option %s\n", argv[optind-1]);
            else
                fprintf(stderr, "error: unknown parsing error\n");
            usage(argv[0], 1);
            break;
        default:
            usage(argv[0], 1);
        }
    }

    // Remaining arguments are input files
    for (int i = optind; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "-") == 0) {
            input_files = realloc(input_files, sizeof(*input_files) * (input_files_count + 1));
            input_files[input_files_count++] = strdup("-");
        } else {
            input_files = realloc(input_files, sizeof(*input_files) * (input_files_count + 1));
            input_files[input_files_count++] = strdup(a);
        }
    }

    if (input_files_count == 0) {
        fprintf(stderr, "error: no input files\n");
        usage((char *)argv[0], 1);
    }

    // Read stdin into temp file if needed
    Token **input_tokens = NULL;
    Obj **input_progs = NULL;
    
    if (input_files_count == 1 && strcmp(input_files[0], "-") == 0) {
        char *tmp = read_stdin_to_tmp();
        if (!tmp) {
            fprintf(stderr, "error: failed to read stdin into temporary file\n");
            exit_code = 1;
            goto BAIL;
        }
        free((void *)input_files[0]);
        input_files[0] = tmp;
    }

    CAST cc;
    cc_init(&cc, 0);

    if (verbose)
        cc.debug_vm = 1;

    // Configure #embed limits
    if (embed_limit > 0) {
        cc.compiler.embed_limit = embed_limit;
        cc.compiler.embed_hard_limit = embed_limit;
    }
    if (embed_hard_error) {
        cc.compiler.embed_hard_error = true;
    }

    cc.collect_errors = true;
    cc.max_errors = max_errors;
    cc.warnings_as_errors = warnings_as_errors;
    jmp_buf err_buf;
    cc.error_jmp_buf = &err_buf;

    if (setjmp(err_buf) != 0) {
        cc_print_all_errors(&cc);
        exit_code = 1;
        goto BAIL;
    }

    // Add include paths
    for (int i = 0; i < inc_paths_count; i++)
        cc_include(&cc, inc_paths[i]);
    for (int i = 0; i < sys_inc_paths_count; i++)
        cc_system_include(&cc, sys_inc_paths[i]);

    // Process macros
    for (int i = 0; i < defines_count; i++)
        parse_define(&cc, (char *)defines[i]);
    for (int i = 0; i < undefs_count; i++)
        cc_undef(&cc, (char *)undefs[i]);

    cc.compiler.skip_preprocess = skip_preprocess;
    input_tokens = calloc(input_files_count, sizeof(Token*));
    for (int i = 0; i < input_files_count; i++) {
        input_tokens[i] = cc_preprocess(&cc, input_files[i]);
        if (!input_tokens[i]) {
            fprintf(stderr, "error: failed to preprocess %s\n", input_files[i]);
            exit_code = 1;
            goto BAIL;
        }
    }

    // Check for preprocessing errors
    if (cc_has_errors(&cc) || cc.warning_count > 0) {
        cc_print_all_errors(&cc);
        if (cc_has_errors(&cc)) {
            exit_code = 1;
            goto BAIL;
        }
    }

    // -E: output preprocessed source
    if (preprocess_only) {
        for (int i = 0; i < input_files_count; i++) {
            FILE *f = out_file ? fopen(out_file, "w") : stdout;
            if (!f) {
                fprintf(stderr, "error: failed to open output file %s\n", out_file);
                exit_code = 1;
                goto BAIL;
            }
            cc_output_preprocessed(f, input_tokens[i]);
            if (f != stdout) fclose(f);
        }
        goto BAIL;
    }

    // -P: print tokens
    if (print_tokens) {
        for (int i = 0; i < input_files_count; i++) {
            printf("=== Tokens for %s ===\n", input_files[i]);
            cc_print_tokens(input_tokens[i]);
            printf("\n");
        }
        goto BAIL;
    }

    // Parse
    input_progs = calloc(input_files_count, sizeof(Obj*));
    for (int i = 0; i < input_files_count; i++) {
        input_progs[i] = cc_parse(&cc, input_tokens[i]);
        if (!input_progs[i]) {
            fprintf(stderr, "error: failed to parse %s\n", input_files[i]);
            exit_code = 1;
            goto BAIL;
        }
    }

    // Check for parsing errors
    if (cc_has_errors(&cc)) {
        cc_print_all_errors(&cc);
        exit_code = 1;
        goto BAIL;
    }

    // Link programs
    Obj *merged_prog = cc_link_progs(&cc, input_progs, input_files_count);
    if (!merged_prog && input_files_count == 1) {
        merged_prog = input_progs[0];
    }

    // -j: JSON output
    if (output_json) {
        FILE *f = out_file ? fopen(out_file, "w") : stdout;
        if (!f) {
            fprintf(stderr, "error: failed to open output file %s\n", out_file);
            exit_code = 1;
            goto BAIL;
        }
        cc_output_json(f, merged_prog);
        if (f != stdout) fclose(f);
        goto BAIL;
    }

    // -a: AST dump
    if (dump_ast) {
        FILE *f = out_file ? fopen(out_file, "w") : stdout;
        if (!f) {
            fprintf(stderr, "error: failed to open output file %s\n", out_file);
            exit_code = 1;
            goto BAIL;
        }
        if (f != stdout) {
            // Redirect stdout temporarily for print_ast
            int saved_stdout = dup(STDOUT_FILENO);
            dup2(fileno(f), STDOUT_FILENO);
            cc_print_ast(&cc, merged_prog);
            fflush(stdout);
            dup2(saved_stdout, STDOUT_FILENO);
            close(saved_stdout);
            fclose(f);
        } else {
            cc_print_ast(&cc, merged_prog);
        }
        goto BAIL;
    }

    // Default: just parse and report success
    printf("Successfully parsed %d file(s)\n", input_files_count);
    if (merged_prog) {
        int fn_count = 0, var_count = 0;
        for (Obj *obj = merged_prog; obj; obj = obj->next) {
            if (obj->is_function) fn_count++;
            else var_count++;
        }
        printf("  Functions: %d\n", fn_count);
        printf("  Variables: %d\n", var_count);
    }

BAIL:
    cc_destroy(&cc);
    if (input_tokens) free(input_tokens);
    if (input_progs) free(input_progs);
    if (out_file) free(out_file);
    if (inc_paths) {
        for (int i = 0; i < inc_paths_count; i++) free((void *)inc_paths[i]);
        free(inc_paths);
    }
    if (sys_inc_paths) {
        for (int i = 0; i < sys_inc_paths_count; i++) free((void *)sys_inc_paths[i]);
        free(sys_inc_paths);
    }
    if (defines) {
        for (int i = 0; i < defines_count; i++) free((void *)defines[i]);
        free(defines);
    }
    if (undefs) {
        for (int i = 0; i < undefs_count; i++) free((void *)undefs[i]);
        free(undefs);
    }
    if (input_files) {
        for (int i = 0; i < input_files_count; i++) free((void *)input_files[i]);
        free(input_files);
    }
    return exit_code;
}
