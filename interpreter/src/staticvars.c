#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "interpreter.h"
#include "common.h"

void sv_add_static_var(it_PROGRAM* program, ts_TYPE* type, char* name) {
    program->static_vars[program->static_var_index].type = type;
    program->static_vars[program->static_var_index].name = name;
    program->static_vars[program->static_var_index].value.number = 0;
    program->static_var_index++;
}
sv_STATIC_VAR* sv_get_var_by_name(it_PROGRAM* program, char* name) {
    for(int i = 0; i < program->static_varsc; i++) {
        if(program->static_vars[i].name == NULL) {
            fatal("Attempt to get variable by name when not all variables are initialized");
        }
        if(strcmp(name, program->static_vars[i].name) == 0) {
            return &program->static_vars[i];
        }
    }
    printf("Static variable name: %s\n", name);
    fatal("Attempt to resolve static variable that doesn't exist");
    return NULL; // Unreachable
}
bool sv_has_var(it_PROGRAM* program, char* name) {
    for(int i = 0; i < program->static_varsc; i++) {
        if(program->static_vars[i].name == NULL) {
            fatal("Attempt to check for existence of static variable by name when not all variables are initialized");
        }
        if(strcmp(name, program->static_vars[i].name) == 0) {
            return true;
        }
    }
    return false;
}
uint32_t sv_get_static_var_index_by_name(it_PROGRAM* program, char* name) {
    for(uint32_t i = 0; i < program->static_varsc; i++) {
        if(program->static_vars[i].name == NULL) {
            fatal("Attempt to get variable index by name when not all variables are initialized");
        }
        if(strcmp(name, program->static_vars[i].name) == 0) {
            return i;
        }
    }
    printf("Static variable name: %s\n", name);
    fatal("Attempt to resolve static variable that doesn't exist");
    return -1; // Unreachable
}