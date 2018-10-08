#include <string.h>
#include <stdlib.h>
#include "interpreter.h"
#include "common.h"

typedef struct ts_TYPE_REGISTRY {
    char* name;
    ts_TYPE* type;
    struct ts_TYPE_REGISTRY* next;
} ts_TYPE_REGISTRY;
static ts_TYPE_REGISTRY* global_registry = NULL;
static uint32_t type_id = 1;
void ts_init_global_registry() {
    // Create and initialize an int type
    global_registry = malloc(sizeof(ts_TYPE_REGISTRY));
    global_registry->name = strdup("int");
    global_registry->type = malloc(sizeof(ts_TYPE));
    ts_TYPE_BAREBONES* int_type = (ts_TYPE_BAREBONES*) global_registry->type;
    int_type->id = 0;
    int_type->heirarchy_len = 1;
    int_type->heirarchy = malloc(sizeof(ts_TYPE_BAREBONES*));
    int_type->heirarchy[0] = (ts_TYPE*) int_type;
    int_type->category = ts_CATEGORY_PRIMITIVE;

    // Create and initialize a bool type
    global_registry->next = malloc(sizeof(ts_TYPE_REGISTRY));
    global_registry->next->next = NULL;
    global_registry->next->name = strdup("bool");
    global_registry->next->type = malloc(sizeof(ts_TYPE));
    ts_TYPE_BAREBONES* bool_type = (ts_TYPE_BAREBONES*) global_registry->next->type;
    bool_type->id = 1;
    bool_type->heirarchy_len = 1;
    bool_type->heirarchy = malloc(sizeof(ts_TYPE_BAREBONES*));
    bool_type->heirarchy[0] = (ts_TYPE*) bool_type;
    bool_type->category = ts_CATEGORY_PRIMITIVE;

    global_registry->next->next = malloc(sizeof(ts_TYPE_REGISTRY));
    global_registry->next->next->next = NULL;
    global_registry->next->next->name = strdup("void");
    global_registry->next->next->type = malloc(sizeof(ts_TYPE));
    ts_TYPE_BAREBONES* void_type = (ts_TYPE_BAREBONES*) global_registry->next->next->type;
    void_type->id = 2;
    void_type->heirarchy_len = 1;
    void_type->heirarchy = malloc(sizeof(ts_TYPE_BAREBONES*));
    void_type->heirarchy[0] = (ts_TYPE*) void_type;
    void_type->category = ts_CATEGORY_PRIMITIVE;
}
uint32_t ts_allocate_type_id() {
    return type_id++;
}
void ts_register_type(ts_TYPE* type, char* name) {
    // CONTRACT: name must be immutable
    if(global_registry == NULL) {
        ts_init_global_registry();
    }
    ts_TYPE_REGISTRY* end = global_registry;
    while(end->next != NULL) {
        end = end->next;
    }
    end->next = malloc(sizeof(ts_TYPE_REGISTRY));
    end->next->name = name;
    end->next->type = type;
    end->next->next = NULL;
}
int ts_get_field_index(ts_TYPE_CLAZZ* clazz, char* name) {
    for(int i = 0; i < clazz->nfields; i++) {
        if(strcmp(clazz->fields[i].name, name) == 0) {
            return i;
        }
    }
    fatal("Unable to find field");
    return -1; // Unreachable
}
int ts_get_method_index(ts_TYPE_CLAZZ* clazz, char* name) {
    for(int i = 0; i < clazz->methodc; i++) {
        if(strcmp(clazz->methods[i]->name, name) == 0) {
            return i;
        }
    }
    fatal("Unable to find class method");
    return -1; // Unreachable
}
ts_TYPE* ts_get_type(char* name) {
    #if DEBUG
    printf("Searching for type '%s'\n", name);
    #endif
    if(global_registry == NULL) {
        ts_init_global_registry();
    }
    ts_TYPE_REGISTRY* registry = global_registry;
    while(registry != NULL) {
        if(strcmp(registry->name, name) == 0) {
            return registry->type;
        }
        registry = registry->next;
    }
    fatal("Unable to find type");
    return NULL; // Unreachable
}
