#include <string.h>
#include <stdlib.h>
#include "typesys.h"
#include "common.h"

typedef struct ts_TYPE_REGISTRY {
    char* name;
    ts_TYPE* type;
    struct ts_TYPE_REGISTRY* next;
} ts_TYPE_REGISTRY;
ts_TYPE_REGISTRY* global_registry = NULL;

void ts_init_global_registry() {
    // Create and initialize an int type
    global_registry = malloc(sizeof(ts_TYPE_REGISTRY));
    global_registry->name = "int";
    global_registry->type = malloc(sizeof(ts_TYPE));
    ts_TYPE_BAREBONES* int_type = (ts_TYPE_BAREBONES*) global_registry->type;
    int_type->id = 0;
    int_type->heirarchy_len = 1;
    int_type->heirarchy = global_registry->type;

    // Create and initialize a bool type
    global_registry->next = malloc(sizeof(ts_TYPE_REGISTRY));
    global_registry->next->next = NULL;
    global_registry->next->name = "bool";
    global_registry->next->type = malloc(sizeof(ts_TYPE));
    ts_TYPE_BAREBONES* bool_type = (ts_TYPE_BAREBONES*) global_registry->next->type;
    bool_type->id = 1;
    bool_type->heirarchy_len = 1;
    bool_type->heirarchy = global_registry->next->type;
}
ts_TYPE* ts_get_type(char* name) {
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
}
