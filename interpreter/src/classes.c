#include <common.h>
#include <interpreter.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

int cl_get_index(struct it_PROGRAM* program, struct ts_TYPE_CLAZZ* type) {
    for(int i = 0; i < program->clazzesc; i++) {
        if(program->clazzes[i] == type) {
            return i;
        }
    }
    return -1;
}
void cl_arrange_phi_tables_inner(struct it_PROGRAM* program, bool* visited, struct ts_TYPE_CLAZZ* type) {
    #if DEBUG
    printf("Arranging phi tables for class %d\n", type->id);
    #endif
    int total_methods = 0;
    if(type->immediate_supertype != NULL) {
        if(!visited[cl_get_index(program, type->immediate_supertype)]) {
            cl_arrange_phi_tables_inner(program, visited, type->immediate_supertype);
        }
        total_methods += type->immediate_supertype->methodc;
    }
    for(int i = 0; i < program->methodc; i++) {
        if(program->methods[i].containing_clazz == type) {
            total_methods++;
        }
    }
    type->methods = mm_malloc(sizeof(struct it_METHOD*) * total_methods);
    int current_method_index = 0;
    if(type->immediate_supertype != NULL) {
        memcpy(type->methods, type->immediate_supertype->methods, sizeof(struct it_METHOD*) * type->immediate_supertype->methodc);
        current_method_index = type->immediate_supertype->methodc;
    }
    for(int i = 0; i < program->methodc; i++) {
        if(program->methods[i].containing_clazz == type) {
            bool foundIt = false;
            if(type->immediate_supertype != NULL) {
                for(int j = 0; j < type->immediate_supertype->methodc; j++) {
                    if(strcmp(type->methods[j]->name, program->methods[i].name) == 0) {
                        foundIt = true;
                        type->methods[j] = &program->methods[i];
                        break;
                    }
                }
            }
            if(!foundIt) {
                type->methods[current_method_index++] = &program->methods[i];
            }
        }
    }
    type->methodc = current_method_index;
    if(type->immediate_supertype != NULL) {
        struct ts_CLAZZ_FIELD* old_fields = type->fields;
        type->fields = mm_malloc(sizeof(struct ts_CLAZZ_FIELD) * (type->nfields + type->immediate_supertype->nfields));
        memcpy(type->fields, type->immediate_supertype->fields, sizeof(struct ts_CLAZZ_FIELD) * type->immediate_supertype->nfields);
        memcpy(type->fields + type->immediate_supertype->nfields, old_fields, sizeof(struct ts_CLAZZ_FIELD) * type->nfields);
        free(old_fields);
        type->nfields += type->immediate_supertype->nfields;
        type->heirarchy_len = type->immediate_supertype->heirarchy_len + 1;
        // Yes, this is supposed to be size of a pointer
        type->heirarchy = malloc(sizeof(union ts_TYPE*) * type->heirarchy_len);
        memcpy(type->heirarchy, type->immediate_supertype->heirarchy, sizeof(union ts_TYPE*) * type->immediate_supertype->heirarchy_len);
        type->heirarchy[type->heirarchy_len - 1] = (union ts_TYPE*) type;
    } else {
        type->heirarchy = malloc(sizeof(union ts_TYPE*) * 1);
        type->heirarchy_len = 1;
        type->heirarchy[0] = (union ts_TYPE*) type;
    }
    int index = cl_get_index(program, type);
    visited[index] = true;
    #if DEBUG
    printf("Finished arranging phi tables for class %d\n", type->id);
    #endif
}
void cl_arrange_phi_tables(struct it_PROGRAM* program) {
    #if DEBUG
    printf("Entering cl_arrange_phi_tables\n");
    #endif
    if(program->clazzesc == 0) {
        return;
    }
    bool visited[program->clazzesc];
    for(int i = 0; i < program->clazzesc; i++) {
        visited[i] = false;
    }
    for(int i = 0; i < program->clazzesc; i++) {
        if(visited[i]) {
            continue;
        }
        cl_arrange_phi_tables_inner(program, visited, program->clazzes[i]);
    }
    #if DEBUG
    printf("Ending cl_arrange_phi_tables\n");
    #endif
}