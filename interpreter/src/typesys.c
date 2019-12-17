#include <string.h>
#include <stdlib.h>
#include "interpreter.h"
#include "common.h"
#include "opcodes.h"
#include "typesys.h"

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
uint32_t ts_get_largest_type_id() {
    // This is only used (at least as of yet) in cl_arrange_method_tables_inner.
    return type_id;
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
bool ts_is_subcontext(struct ts_GENERIC_TYPE_CONTEXT* child, struct ts_GENERIC_TYPE_CONTEXT* parent) {
    if(child == parent) {
        return true;
    }
    if(child->parent != NULL && ts_is_subcontext(child->parent, parent)) {
        return true;
    }
    return false;
}
bool ts_is_from_context(struct ts_TYPE* type, struct ts_GENERIC_TYPE_CONTEXT* context) {
    if(type->category == ts_CATEGORY_TYPE_PARAMETER) {
        return ts_is_subcontext(context, type->data.type_parameter.context);
    } else if(type->category == ts_CATEGORY_ARRAY) {
        return ts_is_from_context(type->data.array.parent_type, context);
    }
    return true;
}
struct ts_TYPE* ts_get_type_by_id_optional(int id) {
    struct ts_TYPE_REGISTRY* registry = global_registry;
    while(registry != NULL) {
        if(registry->type->id == id) {
            return registry->type;
        }
        registry = registry->next;
    }
    return NULL;
}
struct ts_TYPE* ts_get_type_inner(char* name, struct ts_GENERIC_TYPE_CONTEXT* context) {
    #if DEBUG
    printf("Searching for type '%s'\n", name);
    #endif
    if(global_registry == NULL) {
        ts_init_global_registry();
    }
    struct ts_TYPE_REGISTRY* registry = global_registry;
    while(registry != NULL) {
        if(strcmp(registry->name, name) == 0) {
            if(ts_is_from_context(registry->type, context)) {
                return registry->type;
            }
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
    if(context == NULL) {
        return NULL;
    }
    for(int i = 0; i < context->count; i++) {
        if(strcmp(context->arguments[i]->name, name) == 0) {
            return context->arguments[i];
        }
    }
    if(context->parent != NULL) {
        return ts_search_generic_type_context_optional(name, context->parent);
    }
    return NULL;
}
struct ts_TYPE* ts_parse_and_get_type(char* name, struct ts_GENERIC_TYPE_CONTEXT* context, int* const index) {
    // This is an internal function only called recursively and by ts_get_type_optional.
    // It implements a simple recursive descent parser to parse type names.
    // Mixed in here is some logic around actually creating the types. This is
    // kind of unclean.
    const int identifier_start_index = *index;
    if(name[*index] == '[') {
        (*index)++;
        struct ts_TYPE* member_type = ts_parse_and_get_type(name, context, index);
        if(name[(*index)++] != ']') {
            printf("Index: %d, type: '%s': expected ']'", *index, name);
            fatal("Expected ']'");
        }
        return ts_get_or_create_array_type(member_type);
    }
    while(name[*index] != 0) {
        if(name[*index] == ' ') {
            // This isn't perfect, but the compiler doesn't ever emit
            // whitespace anyway.
            (*index)++;
            continue;
        }
        if(name[*index] == ',' || name[*index] == ']' || name[*index] == '>') {
            break;
        }
        if(name[*index] == '<') {
            int raw_type_start_index = identifier_start_index;
            // So that the recursive call to ts_parse_and_get_type sees that the string ends at *index
            name[*index] = '\0';
            struct ts_TYPE* raw_type = ts_parse_and_get_type(name, context, &raw_type_start_index);
            if(raw_type == NULL) {
                return NULL;
            }
            if(raw_type->category != ts_CATEGORY_CLAZZ) {
                fatal("Attempt to specialize something that isn't a class");
            }
            name[*index] = '<';
            (*index)++;
            int type_arguments_index = 0;
            int type_arguments_size = 1;
            struct ts_TYPE** type_arguments = mm_malloc(sizeof(struct ts_TYPE*) * type_arguments_size);
            while(true) {
                struct ts_TYPE* argument_type = ts_parse_and_get_type(name, context, index);
                if(argument_type == NULL) {
                    return NULL;
                }
                if(type_arguments_index >= type_arguments_size) {
                    type_arguments_size *= 2;
                    type_arguments = realloc(type_arguments, sizeof(struct ts_TYPE*) * type_arguments_size);
                }
                type_arguments[type_arguments_index++] = argument_type;
                if(argument_type == NULL) {
                    return NULL;
                }
                if(name[*index] == ',') {
                    (*index)++;
                } else if(name[*index] == '>') {
                    (*index)++;
                    if(raw_type->data.clazz.type_parameters->count == 0) {
                        fatal("Parameterizing non-generic type");
                    }
                    struct ts_TYPE* ret = cl_specialize_class(raw_type, ts_create_type_arguments(raw_type->data.clazz.type_parameters, type_arguments_index, type_arguments));
                    free(type_arguments);
                    return ret;
                } else {
                    printf("Index: %d, character: '%c'\n", *index, name[*index]);
                    fatal("Expected ',' or '>'");
                }
            }
            // Unreachable
        }
        (*index)++;
    }
    char* type_name = mm_malloc(sizeof(char) * ((*index) - identifier_start_index + 1));
    type_name[(*index) - identifier_start_index] = 0;
    memcpy(type_name, name + identifier_start_index, (*index) - identifier_start_index);
    struct ts_TYPE* ret = NULL;
    if(context != NULL) {
        ret = ts_search_generic_type_context_optional(type_name, context);
    }
    if(ret == NULL) {
        ret = ts_get_type_inner(type_name, context);
    }
    memset(type_name, 0, *index - identifier_start_index + 1);
    free(type_name);
    return ret;
}
struct ts_GENERIC_TYPE_CONTEXT* ts_get_method_type_context(struct it_METHOD* method) {
    if(method->containing_clazz != NULL) {
        if(ts_method_is_generic(method)) {
            fatal("Generic method is a class member");
        }
        return method->containing_clazz->data.clazz.type_parameters;
    }
    return method->type_parameters;
}
struct ts_TYPE* ts_get_type_optional(char* name, struct ts_GENERIC_TYPE_CONTEXT* context) {
    if(global_registry == NULL) {
        ts_init_global_registry();
    }
    int index = 0;
    // ts_parse_and_get_type can temporarily modify its input, and we don't
    // know that the memory pointed to by name is mutable.
    char name_copy[strlen(name) + 1];
    name_copy[strlen(name)] = '\0';
    strcpy(name_copy, name);
    return ts_parse_and_get_type(name_copy, context, &index);
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
    struct ts_GENERIC_TYPE_CONTEXT* context = mm_malloc(sizeof(struct ts_GENERIC_TYPE_CONTEXT) + sizeof(struct ts_TYPE*) * num_arguments);
    context->count = num_arguments;
    context->parent = NULL;
    return context;
}
struct ts_TYPE* ts_allocate_type_parameter(char* name, struct ts_GENERIC_TYPE_CONTEXT* context, struct ts_TYPE* extends, int implementsc, struct ts_TYPE** implements) {
    struct ts_TYPE* parameter = mm_malloc(sizeof(struct ts_TYPE));
    parameter->category = ts_CATEGORY_TYPE_PARAMETER;
    parameter->heirarchy = mm_malloc(sizeof(struct ts_TYPE*));
    parameter->heirarchy[0] = parameter;
    parameter->heirarchy_len = 1;
    parameter->id = ts_allocate_type_id();
    parameter->name = name;
    parameter->data.type_parameter.context = context;
    parameter->data.type_parameter.extends = extends;
    parameter->data.type_parameter.implementsc = implementsc;
    parameter->data.type_parameter.implements = mm_malloc(sizeof(struct ts_TYPE*) * implementsc);
    memcpy(parameter->data.type_parameter.implements, implements, sizeof(struct ts_TYPE*) * implementsc);
    return parameter;
}
bool ts_method_is_generic(struct it_METHOD* method) {
    return method->type_parameters->count > 0;
}
bool ts_class_is_specialization(struct ts_TYPE* type) {
    if(type->category != ts_CATEGORY_CLAZZ) {
        fatal("Argument to ts_class_is_specialization isn't a class");
    }
    return type->data.clazz.specialized_from != type;
}
bool ts_class_is_generic(struct ts_TYPE* type) {
    if(type->category != ts_CATEGORY_CLAZZ) {
        fatal("Argument to ts_class_is_generic isn't a class");
    }
    return type->data.clazz.type_parameters->count > 0;
}
bool ts_clazz_is_raw(struct ts_TYPE* type) {
    return ts_class_is_generic(type) && !ts_class_is_specialization(type);
}
struct ts_TYPE_ARGUMENTS* ts_create_type_arguments(struct ts_GENERIC_TYPE_CONTEXT* context, int typeargsc, struct ts_TYPE** typeargs) {
    if(typeargsc != context->count) {
        fatal("ts_create_type_arguments: typeargsc != context->count");
    }
    struct ts_TYPE_ARGUMENTS* result = mm_malloc(sizeof(struct ts_TYPE_ARGUMENTS) + sizeof(result->mapping[0]) * typeargsc);
    result->context = context;
    for(int i = 0; i < typeargsc; i++) {
        result->mapping[i].key = context->arguments[i];
        result->mapping[i].value = typeargs[i];
    }
    return result;
}
struct ts_TYPE_ARGUMENTS* ts_compose_type_arguments(struct ts_TYPE_ARGUMENTS* first, struct ts_TYPE_ARGUMENTS* second) {
    if(first == NULL) {
        return NULL;
    }
    if(second == NULL) {
        return NULL;
    }
    // Some explanation for this in GenericTypeParameters.compose in typesys.py.
    struct ts_TYPE_ARGUMENTS* result = mm_malloc(sizeof(struct ts_TYPE_ARGUMENTS) + sizeof(result->mapping[0]) * first->context->count);
    result->context = first->context;
    for(int i = 0; i < first->context->count; i++) {
        result->mapping[i].key = first[i].mapping[i].key;
        result->mapping[i].value = ts_reify_type(first->mapping[i].value, second);
    }
    return result;
}
bool ts_type_arguments_are_equivalent(struct ts_TYPE_ARGUMENTS* first, struct ts_TYPE_ARGUMENTS* second) {
    // TODO: This is n^2
    if(first->context->count != second->context->count) {
        return false;
    }
    for(int i = 0; i < first->context->count; i++) {
        bool foundIt = false;
        for(int j = 0; j < second->context->count; j++) {
            if(first->mapping[i].key == second->mapping[j].key) {
                if(first->mapping[i].value == second->mapping[j].value) {
                    foundIt = true;
                    break;
                }
                return false;
            }
        }
        if(!foundIt) {
            return false;
        }
    }
    return true;
}
struct ts_TYPE* ts_reify_type(struct ts_TYPE* type, struct ts_TYPE_ARGUMENTS* type_arguments) {
    if(type->category == ts_CATEGORY_PRIMITIVE || type->category == ts_CATEGORY_INTERFACE) {
        return type;
    } else if(type->category == ts_CATEGORY_CLAZZ) {
        if(!ts_class_is_generic(type)) {
            return type;
        }
        if(ts_clazz_is_raw(type)) {
            fatal("Cannot reify a raw type directly");
        }
        struct ts_TYPE_ARGUMENTS* new_arguments = ts_compose_type_arguments(type->data.clazz.type_arguments, type_arguments);
        struct ts_TYPE* result = cl_specialize_class(type, new_arguments);
        free(new_arguments);
        return result;
    } else if(type->category == ts_CATEGORY_ARRAY) {
        struct ts_TYPE* reified = ts_reify_type(type->data.array.parent_type, type_arguments);
        struct ts_TYPE* ret = ts_get_or_create_array_type(reified);
        return ret;
    } else if(type->category == ts_CATEGORY_TYPE_PARAMETER) {
        for(int i = 0; i < type_arguments->context->count; i++) {
            if(type_arguments->mapping[i].key == type) {
                return type_arguments->mapping[i].value;
            }
        }
    }
    printf("Type: %s\n", type->name);
    fatal("Failed to reify type");
    return NULL; // Unreachable
}
void ts_update_method_reification(struct it_METHOD* specialization) {
    struct ts_TYPE_ARGUMENTS* arguments = specialization->typeargs;
    struct it_METHOD* generic_method = specialization->reified_from;

    // TODO: Switch to using indirect method references like we do with type
    // references (so we can use the same opcodes for every instance of a
    // method)? Or is it necessary?
    if(generic_method->opcodes != NULL && specialization->opcodes == NULL) {
        specialization->opcodes = mm_malloc(sizeof(struct it_OPCODE) * generic_method->opcodec);
        memcpy(specialization->opcodes, generic_method->opcodes, sizeof(struct it_OPCODE) * generic_method->opcodec);
    }

    if(generic_method->register_types != NULL && specialization->register_types == NULL) {
        struct ts_TYPE** new_register_types = mm_malloc(sizeof(struct ts_TYPE*) * generic_method->registerc);
        memcpy(new_register_types, generic_method->register_types, sizeof(struct ts_TYPE*) * generic_method->registerc);
        specialization->register_types = new_register_types;
        // Reify register types
        for(int i = 0; i < specialization->registerc; i++) {
            specialization->register_types[i] = ts_reify_type(generic_method->register_types[i], arguments);
        }
    }
    if(generic_method->typereferences != NULL && specialization->typereferences == NULL) {
        specialization->typereferences = mm_malloc(sizeof(struct ts_TYPE*) * generic_method->typereferencec);
        memcpy(specialization->typereferences, generic_method->typereferences, sizeof(struct ts_TYPE*) * generic_method->typereferencec);
        // Reify type references
        for(int i = 0; i < specialization->typereferencec; i++) {
            specialization->typereferences[i] = ts_reify_type(generic_method->typereferences[i], arguments);
        }
    }
    if(generic_method->returntype != NULL && specialization->returntype == NULL) {
        // Reify return type
        // (parameters are reified via register types)
        specialization->returntype = ts_reify_type(generic_method->returntype, arguments);
    }
}
struct it_METHOD* ts_get_method_reification(struct it_METHOD* generic_method, struct ts_TYPE_ARGUMENTS* arguments) {
    if(arguments->context->count <= 0) {
        return generic_method;
    }
    if(generic_method->reified_from != generic_method) {
        if(generic_method->typeargs == NULL) {
            fatal("Invalid state in ts_get_method_reification");
        }
        struct ts_TYPE_ARGUMENTS* final_arguments = ts_compose_type_arguments(generic_method->typeargs, arguments);
        struct it_METHOD* ret = ts_get_method_reification(generic_method->reified_from, final_arguments);
        free(final_arguments);
        return ret;
    }
    if(generic_method->reifications != NULL) {
        for(int i = 0; i < generic_method->reificationsc; i++) {
            // TODO: This code doesn't at all support nested generic type contexts
            // We know reification->typeargs has length typeargsc
            struct it_METHOD* reification = generic_method->reifications[i];
            assert(reification != NULL);
            assert(reification->typeargs != NULL);
            bool compatible = true;
            for(int j = 0; j < arguments->context->count; j++) {
                if(reification->typeargs->mapping[j].value != arguments->mapping[j].value) {
                    compatible = false;
                    break;
                }
            }
            if(compatible) {
                return reification;
            }
        }
    }

    if(generic_method->reifications_size < generic_method->reificationsc + 1) {
        generic_method->reifications_size *= 2;
        if(generic_method->reifications_size < 1) {
            generic_method->reifications_size = 1;
        }
        generic_method->reifications = realloc(generic_method->reifications, sizeof(struct it_METHOD*) * generic_method->reifications_size);
    }

    struct it_METHOD* ret = mm_malloc(sizeof(struct it_METHOD));
    memcpy(ret, generic_method, sizeof(struct it_METHOD));

    if(arguments->context->parent != NULL) {
        fatal("Reifying methods in type contexts with parents isn't supported");
    }

    ret->reified_from = generic_method;

    ret->opcodes = NULL;
    ret->typeargs = NULL;
    ret->register_types = NULL;
    ret->typereferences = NULL;
    ret->returntype = NULL;

    ret->type_parameters = mm_malloc(sizeof(struct ts_GENERIC_TYPE_CONTEXT) + sizeof(struct ts_TYPE*) * 0);
    ret->type_parameters->parent = NULL;
    ret->type_parameters->count = 0;

    // TODO: Create a function or macro to determine this size
    size_t type_args_size = sizeof(struct ts_TYPE_ARGUMENTS) + sizeof(ret->typeargs->mapping[0]) * arguments->context->count;
    ret->typeargs = mm_malloc(type_args_size);
    memcpy(ret->typeargs, arguments, type_args_size);

    ts_update_method_reification(ret);

    generic_method->reifications[generic_method->reificationsc++] = ret;

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
        struct ts_TYPE_ARGUMENTS* type_arguments_struct = ts_create_type_arguments(opcode->data.call.callee->type_parameters, opcode->data.call.type_paramsc, &type_arguments[0]);
        struct it_METHOD* reification = ts_get_method_reification(opcode->data.call.callee, type_arguments_struct);
        free(type_arguments_struct);
        opcode->data.call.callee = reification;
    }
    method->has_had_references_reified = true;
}