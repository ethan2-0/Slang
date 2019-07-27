#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "interpreter_export.h"

union itval rtlib_test(struct it_PROGRAM* program, struct it_STACKFRAME* stackptr, union itval* params) {
    union itval ret;
    ret.number = 5;
    return ret;
}
union itval rtlib_exit(struct it_PROGRAM* program, struct it_STACKFRAME* stackptr, union itval* params) {
    int exit_code = (int) params[0].number;
    exit(exit_code);
    // Unreachable
    union itval ret;
    ret.number = 0;
    return ret;
}
union itval rtlib_print(struct it_PROGRAM* program, struct it_STACKFRAME* stackptr, union itval* params) {
    struct it_ARRAY_DATA* arr = params[0].array_data;
    char* chars = malloc(sizeof(char) * (arr->length + 1));
    for(int i = 0; i < arr->length; i++) {
        chars[i] = (char) (arr->elements[i].number & 0xff);
    }
    chars[arr->length] = 0;
    fputs(chars, stdout);
    free(chars);
    return (union itval) ((int64_t) 0);
}
union itval rtlib_read(struct it_PROGRAM* program, struct it_STACKFRAME* stackptr, union itval* params) {
    struct it_ARRAY_DATA* buff = params[0].array_data;
    int64_t num_chars = params[1].number;
    if(num_chars > 1024) {
        num_chars = 1024;
    }
    // I could use a VLA here, but it wouldn't actualyl help anything
    char mybuff[1025];
    ssize_t result = read(STDIN_FILENO, mybuff, (size_t) num_chars);
    for(int i = 0; i < result; i++) {
        buff->elements[i].number = mybuff[i];
    }
    union itval ret;
    ret.number = result;
    return ret;
}
