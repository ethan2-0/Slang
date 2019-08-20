#include <string.h>
#include <stdlib.h>
#include "interpreter.h"
#include "common.h"
#include "opcodes.h"

struct ts_TYPE_REGISTRY {
    char* name;
    struct ts_TYPE* type;
    struct ts_TYPE_REGISTRY* next;
};
static struct ts_TYPE_REGISTRY* global_registry = NULL;
static uint32_t type_id = 3;
void ts_init_global_registry() {
    // Create and initialize an int type
    global_registry = mm_malloc(sizeof(struct ts_TYPE_REGISTRY));
    global_registry->name = strdup("int");
    global_registry->type = mm_malloc(sizeof(struct ts_TYPE));
    struct ts_TYPE* int_type = global_registry->type;
    int_type->id = 0;
    int_type->name = strdup("int");
    int_type->heirarchy_len = 1;
    int_type->heirarchy = mm_malloc(sizeof(struct ts_TYPE*));
    int_type->heirarchy[0] = int_type;
    int_type->category = ts_CATEGORY_PRIMITIVE;

    // Create and initialize a bool type
    global_registry->next = mm_malloc(sizeof(struct ts_TYPE_REGISTRY));
    global_registry->next->next = NULL;
    global_registry->next->name = strdup("bool");
    global_registry->next->type = mm_malloc(sizeof(struct ts_TYPE));
    struct ts_TYPE* bool_type = global_registry->next->type;
    bool_type->id = 1;
    bool_type->name = strdup("bool");
    bool_type->heirarchy_len = 1;
    bool_type->heirarchy = mm_malloc(sizeof(struct ts_TYPE*));
    bool_type->heirarchy[0] = bool_type;
    bool_type->category = ts_CATEGORY_PRIMITIVE;

    global_registry->next->next = mm_malloc(sizeof(struct ts_TYPE_REGISTRY));
    global_registry->next->next->next = NULL;
    global_registry->next->next->name = strdup("void");
    global_registry->next->next->type = mm_malloc(sizeof(struct ts_TYPE));
    struct ts_TYPE* void_type = global_registry->next->next->type;
    void_type->id = 2;
    bool_type->name = strdup("bool");
    void_type->heirarchy_len = 1;
    void_type->heirarchy = mm_malloc(sizeof(struct ts_TYPE*));
    void_type->heirarchy[0] = void_type;
    void_type->category = ts_CATEGORY_PRIMITIVE;
}
uint32_t ts_allocate_type_id() {
    return type_id++;
}
void ts_register_type(struct ts_TYPE* type, char* name) {
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
struct ts_TYPE* ts_get_type_inner(char* name) {
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
struct ts_TYPE* ts_create_array_type(struct ts_TYPE* parent_type) {
    if(global_registry == NULL) {
        ts_init_global_registry();
    }
    struct ts_TYPE* result = mm_malloc(sizeof(struct ts_TYPE));
    result->category = ts_CATEGORY_ARRAY;
    result->id = ts_allocate_type_id();
    size_t parent_name_length = strlen(parent_type->name);
    result->name = mm_malloc(sizeof(char) * (parent_name_length + 3));
    sprintf(result->name, "[%s]", parent_type->name);
    ts_register_type(result, result->name);
    result->heirarchy_len = 1;
    result->heirarchy = mm_malloc(sizeof(struct ts_TYPE*) * 1);
    result->heirarchy[0] = result;
    result->data.array.parent_type = parent_type;
    ts_register_type(result, result->name);
    return result;
}
struct ts_TYPE* ts_get_or_create_array_type(struct ts_TYPE* parent_type) {
    if(global_registry == NULL) {
        ts_init_global_registry();
    }
    struct ts_TYPE_REGISTRY* registry = global_registry;
    while(registry != NULL) {
        if(registry->type->category != ts_CATEGORY_ARRAY) {
            registry = registry->next;
            continue;
        }
        if(registry->type->data.array.parent_type->id == parent_type->id) {
            return registry->type;
        }
        registry = registry->next;
    }
    return ts_create_array_type(parent_type);
}
struct ts_TYPE* ts_search_generic_type_context_optional(char* name, struct ts_GENERIC_TYPE_CONTEXT* context) {
    for(int i = 0; i < context->count; i++) {
        if(strcmp(context->arguments[i].name, name) == 0) {
            return &context->arguments[i];
        }
    }
    if(context->parent != NULL) {
        return ts_search_generic_type_context_optional(name, context->parent);
    }
    return NULL;
}
struct ts_TYPE* ts_get_type_optional(char* name, struct ts_GENERIC_TYPE_CONTEXT* context) {
    if(global_registry == NULL) {
        ts_init_global_registry();
    }
    struct ts_TYPE* typ = NULL;
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
            struct ts_TYPE* ret = ts_create_array_type(ts_get_type(parent_name + 1, context));
            free(parent_name);
            #if DEBUG
            printf("Created array type with ID %d\n", ret->id);
            #endif
            return ret;
        }
    }
    return typ;
}
struct ts_TYPE* ts_get_type(char* name, struct ts_GENERIC_TYPE_CONTEXT* context) {
    if(global_registry == NULL) {
        ts_init_global_registry();
    }
    struct ts_TYPE* result = ts_get_type_optional(name, context);
    if(result == NULL) {
        printf("Tried to find type: '%s'\n", name);
        fatal("Cannot find type");
    }
    return result;
}
bool ts_is_compatible(struct ts_TYPE* type1, struct ts_TYPE* type2) {
    if(global_registry == NULL) {
        ts_init_global_registry();
    }
    if(type1->category == ts_CATEGORY_INTERFACE) {
        return type1 == type2;
    }
    if(type2->category == ts_CATEGORY_INTERFACE) {
        if(type1->category != ts_CATEGORY_CLAZZ) {
            return false;
        }
        return cl_class_implements_interface(type1, type2);
    }
    int min_heirarchy_len = type1->heirarchy_len;
    if(type2->heirarchy_len < min_heirarchy_len) {
        min_heirarchy_len = type2->heirarchy_len;
    }
    return type1->heirarchy[min_heirarchy_len] == type2->heirarchy[min_heirarchy_len];
}
bool ts_instanceof(struct ts_TYPE* lhs, struct ts_TYPE* rhs) {
    if(global_registry == NULL) {
        ts_init_global_registry();
    }
    // Test if lhs instanceof rhs
    #if DEBUG
    printf("Asked if %s instanceof %s\n", lhs->name, rhs->name);
    #endif
    if(lhs->category == ts_CATEGORY_ARRAY) {
        if(rhs->category != ts_CATEGORY_ARRAY) {
            return false;
        }
        // There's only ever one instance of an array type, so this is correct
        return lhs == rhs;
    }
    if(rhs->category == ts_CATEGORY_INTERFACE) {
        if(lhs->category != ts_CATEGORY_CLAZZ) {
            return false;
        }
        return cl_class_implements_interface(lhs, rhs);
    }
    if(lhs->heirarchy_len < rhs->heirarchy_len) {
        return false;
    }
    #if DEBUG
    printf("rhs->heirarchy_len: %d\n", rhs->heirarchy_len);
    printf("Types: %s and %s\n", lhs->heirarchy[rhs->heirarchy_len - 1]->name, rhs->heirarchy[rhs->heirarchy_len - 1]->name);
    #endif
    return lhs->heirarchy[rhs->heirarchy_len - 1] == rhs->heirarchy[rhs->heirarchy_len - 1];
}
struct ts_GENERIC_TYPE_CONTEXT* ts_create_generic_type_context(uint32_t num_arguments, struct ts_GENERIC_TYPE_CONTEXT* parent) {
    struct ts_GENERIC_TYPE_CONTEXT* context = mm_malloc(sizeof(struct ts_GENERIC_TYPE_CONTEXT) + sizeof(struct ts_TYPE) * num_arguments);
    context->count = num_arguments;
    context->parent = NULL;
    return context;
}
void ts_init_type_parameter(struct ts_TYPE* parameter, char* name) {
    parameter->category = ts_CATEGORY_TYPE_PARAMETER;
    parameter->heirarchy = mm_malloc(sizeof(struct ts_TYPE*));
    parameter->heirarchy[0] = parameter;
    parameter->heirarchy_len = 1;
    parameter->id = ts_allocate_type_id();
    parameter->name = name;
}
bool ts_method_is_generic(struct it_METHOD* method) {
    return method->type_parameters->count > 0;
}
struct ts_TYPE* ts_reify_type(struct ts_TYPE* type, struct ts_GENERIC_TYPE_CONTEXT* context, int typeargsc, struct ts_TYPE** typeargs) {
    if(typeargsc != context->count) {
        fatal("ts_reify_type: typeargsc != context->count");
    }
    if(type->category == ts_CATEGORY_PRIMITIVE || type->category == ts_CATEGORY_CLAZZ || type->category == ts_CATEGORY_INTERFACE) {
        return type;
    } else if(type->category == ts_CATEGORY_ARRAY) {
        struct ts_TYPE* reified = ts_reify_type(type->data.array.parent_type, context, typeargsc, typeargs);
        struct ts_TYPE* ret = ts_get_or_create_array_type(reified);
        return ret;
    } else if(type->category == ts_CATEGORY_TYPE_PARAMETER) {
        for(int i = 0; i < typeargsc; i++) {
            if(&context->arguments[i] == type) {
                return typeargs[i];
            }
        }
    }
    printf("Type: %s\n", type->name);
    fatal("Failed to reify type");
    return NULL; // Unreachable
}
struct it_METHOD* ts_get_method_reification(struct it_METHOD* generic_method, int typeargsc, struct ts_TYPE** typeargs) {
    struct it_METHOD* ret = mm_malloc(sizeof(struct it_METHOD));
    memcpy(ret, generic_method, sizeof(struct it_METHOD));

    if(generic_method->type_parameters->parent != NULL) {
        fatal("Attempt to reify a method whose generic type context has a parent");
    }

    struct ts_TYPE** new_register_types = mm_malloc(sizeof(struct ts_TYPE*) * ret->registerc);
    memcpy(new_register_types, ret->register_types, sizeof(struct ts_TYPE*) * ret->registerc);
    ret->register_types = new_register_types;

    ret->type_parameters = mm_malloc(sizeof(struct ts_GENERIC_TYPE_CONTEXT));
    ret->type_parameters->parent = NULL;
    ret->type_parameters->count = 0;

    // TODO: Switch to using indirect method references like we do with type
    // references (so we can use the same opcodes for every instance of a
    // method)? Or is it necessary?
    ret->opcodes = mm_malloc(sizeof(struct it_OPCODE) * ret->opcodec);
    memcpy(ret->opcodes, generic_method->opcodes, sizeof(struct it_OPCODE) * ret->opcodec);

    ret->typereferences = mm_malloc(sizeof(struct ts_TYPE*) * ret->typereferencec);
    memcpy(ret->typereferences, generic_method->typereferences, sizeof(struct ts_TYPE*) * ret->typereferencec);

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
        if(!ts_method_is_generic(opcode->data.call.callee)) {
            continue;
        }
        // I could use a VLA here but it's not necessary
        struct ts_TYPE* type_arguments[TS_MAX_TYPE_ARGS];
        for(int i = 0; i < opcode->data.call.type_paramsc; i++) {
            type_arguments[i] = method->typereferences[opcode->data.call.type_params[i]];
        }
        struct it_METHOD* reification = ts_get_method_reification(opcode->data.call.callee, opcode->data.call.type_paramsc, type_arguments);
        opcode->data.call.callee = reification;
    }
    method->has_had_references_reified = true;
}