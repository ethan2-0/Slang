#include <string.h>
#include <stdlib.h>
#include "interpreter.h"
#include "common.h"
#include "opcodes.h"

struct ts_TYPE_REGISTRY {
    char* name;
    union ts_TYPE* type;
    struct ts_TYPE_REGISTRY* next;
};
static struct ts_TYPE_REGISTRY* global_registry = NULL;
static uint32_t type_id = 3;
void ts_init_global_registry() {
    // Create and initialize an int type
    global_registry = mm_malloc(sizeof(struct ts_TYPE_REGISTRY));
    global_registry->name = strdup("int");
    global_registry->type = mm_malloc(sizeof(union ts_TYPE));
    struct ts_TYPE_BAREBONES* int_type = (struct ts_TYPE_BAREBONES*) global_registry->type;
    int_type->id = 0;
    int_type->name = strdup("int");
    int_type->heirarchy_len = 1;
    int_type->heirarchy = mm_malloc(sizeof(struct ts_TYPE_BAREBONES*));
    int_type->heirarchy[0] = (union ts_TYPE*) int_type;
    int_type->category = ts_CATEGORY_PRIMITIVE;

    // Create and initialize a bool type
    global_registry->next = mm_malloc(sizeof(struct ts_TYPE_REGISTRY));
    global_registry->next->next = NULL;
    global_registry->next->name = strdup("bool");
    global_registry->next->type = mm_malloc(sizeof(union ts_TYPE));
    struct ts_TYPE_BAREBONES* bool_type = (struct ts_TYPE_BAREBONES*) global_registry->next->type;
    bool_type->id = 1;
    bool_type->name = strdup("bool");
    bool_type->heirarchy_len = 1;
    bool_type->heirarchy = mm_malloc(sizeof(struct ts_TYPE_BAREBONES*));
    bool_type->heirarchy[0] = (union ts_TYPE*) bool_type;
    bool_type->category = ts_CATEGORY_PRIMITIVE;

    global_registry->next->next = mm_malloc(sizeof(struct ts_TYPE_REGISTRY));
    global_registry->next->next->next = NULL;
    global_registry->next->next->name = strdup("void");
    global_registry->next->next->type = mm_malloc(sizeof(union ts_TYPE));
    struct ts_TYPE_BAREBONES* void_type = (struct ts_TYPE_BAREBONES*) global_registry->next->next->type;
    void_type->id = 2;
    bool_type->name = strdup("bool");
    void_type->heirarchy_len = 1;
    void_type->heirarchy = mm_malloc(sizeof(struct ts_TYPE_BAREBONES*));
    void_type->heirarchy[0] = (union ts_TYPE*) void_type;
    void_type->category = ts_CATEGORY_PRIMITIVE;
}
uint32_t ts_allocate_type_id() {
    return type_id++;
}
void ts_register_type(union ts_TYPE* type, char* name) {
    // CONTRACT: name must be immutable
    if(global_registry == NULL) {
        ts_init_global_registry();
    }
    struct ts_TYPE_REGISTRY* end = global_registry;
    while(end->next != NULL) {
        end = end->next;
    }
    end->next = mm_malloc(sizeof(struct ts_TYPE_REGISTRY));
    end->next->name = name;
    end->next->type = type;
    end->next->next = NULL;
}
union ts_TYPE* ts_get_type_inner(char* name) {
    #if DEBUG
    printf("Searching for type '%s'\n", name);
    #endif
    if(global_registry == NULL) {
        ts_init_global_registry();
    }
    struct ts_TYPE_REGISTRY* registry = global_registry;
    while(registry != NULL) {
        if(strcmp(registry->name, name) == 0) {
            return registry->type;
        }
        registry = registry->next;
    }
    return NULL;
}
struct ts_TYPE_ARRAY* ts_create_array_type(union ts_TYPE* parent_type) {
    if(global_registry == NULL) {
        ts_init_global_registry();
    }
    struct ts_TYPE_ARRAY* result = mm_malloc(sizeof(struct ts_TYPE_ARRAY));
    result->category = ts_CATEGORY_ARRAY;
    result->id = ts_allocate_type_id();
    size_t parent_name_length = strlen(parent_type->barebones.name);
    result->name = mm_malloc(sizeof(char) * (parent_name_length + 3));
    sprintf(result->name, "[%s]", parent_type->barebones.name);
    ts_register_type((union ts_TYPE*) result, result->name);
    result->heirarchy_len = 1;
    result->heirarchy = mm_malloc(sizeof(union ts_TYPE*) * 1);
    result->heirarchy[0] = (union ts_TYPE*) result;
    result->parent_type = parent_type;
    ts_register_type((union ts_TYPE*) result, result->name);
    return result;
}
struct ts_TYPE_ARRAY* ts_get_or_create_array_type(union ts_TYPE* parent_type) {
    if(global_registry == NULL) {
        ts_init_global_registry();
    }
    struct ts_TYPE_REGISTRY* registry = global_registry;
    while(registry != NULL) {
        if(registry->type->barebones.category != ts_CATEGORY_ARRAY) {
            registry = registry->next;
            continue;
        }
        if(registry->type->array.parent_type->barebones.id == parent_type->barebones.id) {
            return (struct ts_TYPE_ARRAY*) registry->type;
        }
        registry = registry->next;
    }
    return ts_create_array_type(parent_type);
}
union ts_TYPE* ts_search_generic_type_context_optional(char* name, struct ts_GENERIC_TYPE_CONTEXT* context) {
    for(int i = 0; i < context->count; i++) {
        if(strcmp(context->arguments[i].name, name) == 0) {
            return (union ts_TYPE*) &context->arguments[i];
        }
    }
    if(context->parent != NULL) {
        return ts_search_generic_type_context_optional(name, context->parent);
    }
    return NULL;
}
union ts_TYPE* ts_get_type_optional(char* name, struct ts_GENERIC_TYPE_CONTEXT* context) {
    if(global_registry == NULL) {
        ts_init_global_registry();
    }
    union ts_TYPE* typ = NULL;
    if(context != NULL) {
        typ = ts_search_generic_type_context_optional(name, context);
    }
    if(typ == NULL) {
        typ = ts_get_type_inner(name);
    }
    if(typ == NULL) {
        if(name[0] == '[') {
            #if DEBUG
            printf("About to create array type '%s'\n", name);
            #endif
            char* parent_name = strdup(name);
            // Remove the last character (which is `]`)
            parent_name[strlen(parent_name) - 1] = '\0';
            union ts_TYPE* ret = (union ts_TYPE*) ts_create_array_type(ts_get_type(parent_name + 1, context));
            free(parent_name);
            #if DEBUG
            printf("Created array type with ID %d\n", ret->barebones.id);
            #endif
            return ret;
        }
    }
    return typ;
}
union ts_TYPE* ts_get_type(char* name, struct ts_GENERIC_TYPE_CONTEXT* context) {
    if(global_registry == NULL) {
        ts_init_global_registry();
    }
    union ts_TYPE* result = ts_get_type_optional(name, context);
    if(result == NULL) {
        printf("Tried to find type: '%s'\n", name);
        fatal("Cannot find type");
    }
    return result;
}
bool ts_is_compatible(union ts_TYPE* type1, union ts_TYPE* type2) {
    if(global_registry == NULL) {
        ts_init_global_registry();
    }
    if(type1->barebones.category == ts_CATEGORY_INTERFACE) {
        return type1 == type2;
    }
    if(type2->barebones.category == ts_CATEGORY_INTERFACE) {
        if(type1->barebones.category != ts_CATEGORY_CLAZZ) {
            return false;
        }
        return cl_class_implements_interface(&type1->clazz, &type2->interface);
    }
    int min_heirarchy_len = type1->barebones.heirarchy_len;
    if(type2->barebones.heirarchy_len < min_heirarchy_len) {
        min_heirarchy_len = type2->barebones.heirarchy_len;
    }
    return type1->barebones.heirarchy[min_heirarchy_len] == type2->barebones.heirarchy[min_heirarchy_len];
}
bool ts_instanceof(union ts_TYPE* lhs, union ts_TYPE* rhs) {
    if(global_registry == NULL) {
        ts_init_global_registry();
    }
    // Test if lhs instanceof rhs
    #if DEBUG
    printf("Asked if %s instanceof %s\n", lhs->barebones.name, rhs->barebones.name);
    #endif
    if(lhs->barebones.category == ts_CATEGORY_ARRAY) {
        if(rhs->barebones.category != ts_CATEGORY_ARRAY) {
            return false;
        }
        // There's only ever one instance of an array type, so this is correct
        return lhs == rhs;
    }
    if(rhs->barebones.category == ts_CATEGORY_INTERFACE) {
        if(lhs->barebones.category != ts_CATEGORY_CLAZZ) {
            return false;
        }
        return cl_class_implements_interface((struct ts_TYPE_CLAZZ*) lhs, (struct ts_TYPE_INTERFACE*) rhs);
    }
    if(lhs->barebones.heirarchy_len < rhs->barebones.heirarchy_len) {
        return false;
    }
    #if DEBUG
    printf("rhs->heirarchy_len: %d\n", rhs->barebones.heirarchy_len);
    printf("Types: %s and %s\n", lhs->barebones.heirarchy[rhs->barebones.heirarchy_len - 1]->barebones.name, rhs->barebones.heirarchy[rhs->barebones.heirarchy_len - 1]->barebones.name);
    #endif
    return lhs->barebones.heirarchy[rhs->barebones.heirarchy_len - 1] == rhs->barebones.heirarchy[rhs->barebones.heirarchy_len - 1];
}
struct ts_GENERIC_TYPE_CONTEXT* ts_create_generic_type_context(uint32_t num_arguments, struct ts_GENERIC_TYPE_CONTEXT* parent) {
    struct ts_GENERIC_TYPE_CONTEXT* context = mm_malloc(sizeof(struct ts_GENERIC_TYPE_CONTEXT) + sizeof(struct ts_TYPE_PARAMETER) * num_arguments);
    context->count = num_arguments;
    context->parent = NULL;
    return context;
}
void ts_init_type_parameter(struct ts_TYPE_PARAMETER* parameter, char* name) {
    parameter->category = ts_CATEGORY_TYPE_PARAMETER;
    parameter->heirarchy = mm_malloc(sizeof(union ts_TYPE*));
    parameter->heirarchy[0] = (union ts_TYPE*) parameter;
    parameter->heirarchy_len = 1;
    parameter->id = ts_allocate_type_id();
    parameter->name = name;
}
bool ts_method_is_generic(struct it_METHOD* method) {
    return method->type_parameters->count > 0;
}
union ts_TYPE* ts_reify_type(union ts_TYPE* type, struct ts_GENERIC_TYPE_CONTEXT* context, int typeargsc, union ts_TYPE** typeargs) {
    if(typeargsc != context->count) {
        fatal("ts_reify_type: typeargsc != context->count");
    }
    if(type->barebones.category == ts_CATEGORY_PRIMITIVE || type->barebones.category == ts_CATEGORY_CLAZZ || type->barebones.category == ts_CATEGORY_INTERFACE) {
        return type;
    } else if(type->barebones.category == ts_CATEGORY_ARRAY) {
        union ts_TYPE* reified = ts_reify_type(type->array.parent_type, context, typeargsc, typeargs);
        union ts_TYPE* ret = (union ts_TYPE*) ts_get_or_create_array_type(reified);
        return ret;
    } else if(type->barebones.category == ts_CATEGORY_TYPE_PARAMETER) {
        for(int i = 0; i < typeargsc; i++) {
            if(((union ts_TYPE*) &context->arguments[i]) == type) {
                return typeargs[i];
            }
        }
    }
    printf("Type: %s\n", type->barebones.name);
    fatal("Failed to reify type");
    return NULL; // Unreachable
}
struct it_METHOD* ts_get_method_reification(struct it_METHOD* generic_method, int typeargsc, union ts_TYPE** typeargs) {
    // for(int i = 0; i < generic_method->reificationsc; i++) {
    //     struct it_METHOD* reified_method = generic_method->reifications[i];
    // }

    if(typeargsc != generic_method->type_parameters->count) {
        fatal("uh oh");
    }

    struct it_METHOD* ret = mm_malloc(sizeof(struct it_METHOD));
    memcpy(ret, generic_method, sizeof(struct it_METHOD));

    if(generic_method->type_parameters->parent != NULL) {
        fatal("Attempt to reify a method whose generic type context has a parent");
    }

    union ts_TYPE** new_register_types = mm_malloc(sizeof(union ts_TYPE*) * ret->registerc);
    memcpy(new_register_types, ret->register_types, sizeof(union ts_TYPE*) * ret->registerc);
    ret->register_types = new_register_types;

    ret->type_parameters = mm_malloc(sizeof(struct ts_GENERIC_TYPE_CONTEXT));
    ret->type_parameters->parent = NULL;
    ret->type_parameters->count = 0;

    // TODO: Switch to using indirect method references like we do with type
    // references (so we can use the same opcodes for every instance of a
    // method)? Or is it necessary?
    ret->opcodes = mm_malloc(sizeof(struct it_OPCODE) * ret->opcodec);
    memcpy(ret->opcodes, generic_method->opcodes, sizeof(struct it_OPCODE) * ret->opcodec);

    ret->typereferences = mm_malloc(sizeof(union ts_TYPE*) * ret->typereferencec);
    memcpy(ret->typereferences, generic_method->typereferences, sizeof(union ts_TYPE*) * ret->typereferencec);

    // Reify register types
    for(int i = 0; i < ret->registerc; i++) {
        ret->register_types[i] = ts_reify_type(ret->register_types[i], generic_method->type_parameters, typeargsc, typeargs);
    }
    // Reify type references
    for(int i = 0; i < ret->typereferencec; i++) {
        ret->typereferences[i] = ts_reify_type(ret->typereferences[i], generic_method->type_parameters, typeargsc, typeargs);
    }
    // Reify return type
    // (parameters are reified via register types)
    ret->returntype = ts_reify_type(ret->returntype, generic_method->type_parameters, typeargsc, typeargs);

    return ret;
}
void ts_reify_generic_references(struct it_METHOD* method) {
    if(ts_method_is_generic(method)) {
        // We shouldn't need this check but it won't hurt
        return;
    }
    for(int i = 0; i < method->opcodec; i++) {
        struct it_OPCODE* opcode = &method->opcodes[i];
        if(opcode->type != OPCODE_CALL) {
            continue;
        }
        struct it_OPCODE_DATA_CALL* data = (struct it_OPCODE_DATA_CALL*) opcode->payload;
        if(!ts_method_is_generic(data->callee)) {
            continue;
        }
        // I could use a VLA here but it's not necessary
        union ts_TYPE* type_arguments[TS_MAX_TYPE_ARGS];
        for(int i = 0; i < data->type_paramsc; i++) {
            type_arguments[i] = method->typereferences[data->type_params[i]];
        }
        struct it_METHOD* reification = ts_get_method_reification(data->callee, data->type_paramsc, type_arguments);
        opcode->payload = mm_malloc(sizeof(struct it_OPCODE_DATA_CALL));
        memcpy(opcode->payload, data, sizeof(struct it_OPCODE_DATA_CALL));
        ((struct it_OPCODE_DATA_CALL*) opcode->payload)->callee = reification;
    }
    method->has_had_references_reified = true;
}