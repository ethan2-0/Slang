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
static uint32_t type_id = 3;
void ts_init_global_registry() {
    // Create and initialize an int type
    global_registry = mm_malloc(sizeof(ts_TYPE_REGISTRY));
    global_registry->name = strdup("int");
    global_registry->type = mm_malloc(sizeof(ts_TYPE));
    ts_TYPE_BAREBONES* int_type = (ts_TYPE_BAREBONES*) global_registry->type;
    int_type->id = 0;
    int_type->name = strdup("int");
    int_type->heirarchy_len = 1;
    int_type->heirarchy = mm_malloc(sizeof(ts_TYPE_BAREBONES*));
    int_type->heirarchy[0] = (ts_TYPE*) int_type;
    int_type->category = ts_CATEGORY_PRIMITIVE;

    // Create and initialize a bool type
    global_registry->next = mm_malloc(sizeof(ts_TYPE_REGISTRY));
    global_registry->next->next = NULL;
    global_registry->next->name = strdup("bool");
    global_registry->next->type = mm_malloc(sizeof(ts_TYPE));
    ts_TYPE_BAREBONES* bool_type = (ts_TYPE_BAREBONES*) global_registry->next->type;
    bool_type->id = 1;
    bool_type->name = strdup("bool");
    bool_type->heirarchy_len = 1;
    bool_type->heirarchy = mm_malloc(sizeof(ts_TYPE_BAREBONES*));
    bool_type->heirarchy[0] = (ts_TYPE*) bool_type;
    bool_type->category = ts_CATEGORY_PRIMITIVE;

    global_registry->next->next = mm_malloc(sizeof(ts_TYPE_REGISTRY));
    global_registry->next->next->next = NULL;
    global_registry->next->next->name = strdup("void");
    global_registry->next->next->type = mm_malloc(sizeof(ts_TYPE));
    ts_TYPE_BAREBONES* void_type = (ts_TYPE_BAREBONES*) global_registry->next->next->type;
    void_type->id = 2;
    bool_type->name = strdup("bool");
    void_type->heirarchy_len = 1;
    void_type->heirarchy = mm_malloc(sizeof(ts_TYPE_BAREBONES*));
    void_type->heirarchy[0] = (ts_TYPE*) void_type;
    void_type->category = ts_CATEGORY_PRIMITIVE;
}
uint32_t ts_allocate_type_id() {
    // The whole concept of type IDs isn't actually necessary
    // I could replace this with `return 0` and all the tests would pass
    // But who knows, it could be useful, so I'll keep it
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
    end->next = mm_malloc(sizeof(ts_TYPE_REGISTRY));
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
    #if DEBUG
    printf("Field name: '%s'\n", name);
    #endif
    fatal("Unable to find field");
    return -1; // Unreachable
}
int ts_get_method_index(ts_TYPE_CLAZZ* clazz, char* name) {
    #if DEBUG
    printf("Asked to get index of %llx's field %s\n", clazz, name);
    printf("%d %llx\n", clazz->methodc, clazz->methods);
    #endif
    for(int i = 0; i < clazz->methodc; i++) {
        if(strcmp(clazz->methods[i]->name, name) == 0) {
            return i;
        }
    }
    fatal("Unable to find class method");
    return -1; // Unreachable
}
ts_TYPE* ts_get_type_inner(char* name) {
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
    return NULL;
}
ts_TYPE_ARRAY* ts_create_array_type(ts_TYPE* parent_type) {
    ts_TYPE_ARRAY* result = mm_malloc(sizeof(ts_TYPE_ARRAY));
    result->category = ts_CATEGORY_ARRAY;
    result->id = ts_allocate_type_id();
    size_t parent_name_length = strlen(parent_type->barebones.name);
    result->name = mm_malloc(sizeof(char) * (parent_name_length + 3));
    sprintf(result->name, "[%s]", parent_type->barebones.name);
    ts_register_type((ts_TYPE*) result, result->name);
    result->heirarchy_len = 1;
    result->heirarchy = mm_malloc(sizeof(ts_TYPE*) * 1);
    result->heirarchy[0] = (ts_TYPE*) result;
    result->parent_type = parent_type;
    ts_register_type((ts_TYPE*) result, result->name);
    return result;
}
ts_TYPE* ts_get_type(char* name) {
    ts_TYPE* typ = ts_get_type_inner(name);
    if(typ == NULL) {
        if(name[0] == '[') {
            #if DEBUG
            printf("About to create array type '%s'\n", name);
            #endif
            char* parent_name = strdup(name);
            // Remove the last character (which is `]`)
            parent_name[strlen(parent_name) - 1] = '\0';
            ts_TYPE* ret = (ts_TYPE*) ts_create_array_type(ts_get_type(parent_name + 1));
            free(parent_name);
            #if DEBUG
            printf("Created array type with ID %d\n", ret->barebones.id);
            #endif
            return ret;
        }
        printf("Tried to find type: '%s'\n", name);
        fatal("Cannot find type");
    }
    return typ;
}
bool ts_is_compatible(ts_TYPE* type1, ts_TYPE* type2) {
    int min_heirarchy_len = type1->barebones.heirarchy_len;
    if(type2->barebones.heirarchy_len < min_heirarchy_len) {
        min_heirarchy_len = type2->barebones.heirarchy_len;
    }
    return type1->barebones.heirarchy[min_heirarchy_len] == type2->barebones.heirarchy[min_heirarchy_len];
}
bool ts_instanceof(ts_TYPE* lhs, ts_TYPE* rhs) {
    // Test if lhs instanceof rhs
    #if DEBUG
    printf("Asked if %s instanceof %s\n", lhs->barebones.name, rhs->barebones.name);
    #endif
    if(lhs->barebones.heirarchy_len < rhs->barebones.heirarchy_len) {
        return false;
    }
    #if DEBUG
    printf("rhs->heirarchy_len: %d\n", rhs->barebones.heirarchy_len);
    printf("Types: %s and %s\n", lhs->barebones.heirarchy[rhs->barebones.heirarchy_len - 1]->barebones.name, rhs->barebones.heirarchy[rhs->barebones.heirarchy_len - 1]->barebones.name);
    #endif
    return lhs->barebones.heirarchy[rhs->barebones.heirarchy_len - 1] == rhs->barebones.heirarchy[rhs->barebones.heirarchy_len - 1];
}