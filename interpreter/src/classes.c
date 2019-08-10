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
void cl_arrange_method_tables_inner(struct it_PROGRAM* program, bool* visited, struct ts_TYPE_CLAZZ* type) {
    #if DEBUG
    printf("Arranging phi tables for class %d\n", type->id);
    #endif
    int total_methods = 0;
    if(type->immediate_supertype != NULL) {
        if(!visited[cl_get_index(program, type->immediate_supertype)]) {
            cl_arrange_method_tables_inner(program, visited, type->immediate_supertype);
        }
        total_methods += type->immediate_supertype->method_table->nmethods;
    }
    for(int i = 0; i < program->methodc; i++) {
        if(program->methods[i].containing_clazz == type) {
            total_methods++;
        }
    }
    type->method_table = mm_malloc(sizeof(struct it_METHOD_TABLE) + sizeof(struct it_METHOD*) * total_methods);
    type->method_table->type = (union ts_TYPE*) type;
    type->method_table->category = type->category;
    int current_method_index = 0;
    if(type->immediate_supertype != NULL) {
        memcpy(&type->method_table->methods, &type->immediate_supertype->method_table->methods, sizeof(struct it_METHOD*) * type->immediate_supertype->method_table->nmethods);
        current_method_index = type->immediate_supertype->method_table->nmethods;
    }
    for(int i = 0; i < program->methodc; i++) {
        if(program->methods[i].containing_clazz == type) {
            bool foundIt = false;
            if(type->immediate_supertype != NULL) {
                for(int j = 0; j < type->immediate_supertype->method_table->nmethods; j++) {
                    if(strcmp(type->method_table->methods[j]->name, program->methods[i].name) == 0) {
                        foundIt = true;
                        type->method_table->methods[j] = &program->methods[i];
                        break;
                    }
                }
            }
            if(!foundIt) {
                type->method_table->methods[current_method_index++] = &program->methods[i];
            }
        }
    }
    type->method_table->nmethods = current_method_index;
    if(type->immediate_supertype != NULL) {
        struct ts_CLAZZ_FIELD* old_fields = type->fields;
        type->fields = mm_malloc(sizeof(struct ts_CLAZZ_FIELD) * (type->nfields + type->immediate_supertype->nfields));
        memcpy(type->fields, type->immediate_supertype->fields, sizeof(struct ts_CLAZZ_FIELD) * type->immediate_supertype->nfields);
        memcpy(type->fields + type->immediate_supertype->nfields, old_fields, sizeof(struct ts_CLAZZ_FIELD) * type->nfields);
        free(old_fields);
        type->nfields += type->immediate_supertype->nfields;
        type->heirarchy_len = type->immediate_supertype->heirarchy_len + 1;
        // Yes, this is supposed to be size of a pointer
        free(type->heirarchy);
        type->heirarchy = malloc(sizeof(union ts_TYPE*) * type->heirarchy_len);
        memcpy(type->heirarchy, type->immediate_supertype->heirarchy, sizeof(union ts_TYPE*) * type->immediate_supertype->heirarchy_len);
        type->heirarchy[type->heirarchy_len - 1] = (union ts_TYPE*) type;
    } else {
        free(type->heirarchy);
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
void cl_arrange_interface_methods(struct it_PROGRAM* program) {
    for(int i = 0; i < program->interfacesc; i++) {
        struct ts_TYPE_INTERFACE* interface = program->interfaces[i];
        int num_methods = 0;
        for(int j = 0; j < program->methodc; j++) {
            if(program->methods[j].containing_interface == interface) {
                num_methods++;
            }
        }
        interface->methods = mm_malloc(sizeof(struct it_METHOD_TABLE) + sizeof(struct it_METHOD*) * num_methods);
        interface->methods->category = ts_CATEGORY_INTERFACE;
        interface->methods->type = (union ts_TYPE*) interface;
        interface->methods->interface_implementations = NULL;
        interface->methods->ninterface_implementations = 0;
        interface->methods->nmethods = num_methods;
        int index = 0;
        for(int j = 0; j < program->methodc; j++) {
            if(program->methods[j].containing_interface == interface) {
                interface->methods->methods[index++] = &program->methods[j];
            }
        }
    }
}
void cl_arrange_interface_implementations(struct it_PROGRAM* program) {
    for(int i = 0; i < program->clazzesc; i++) {
        struct ts_TYPE_CLAZZ* clazz = program->clazzes[i];
        clazz->method_table->ninterface_implementations = clazz->implemented_interfacesc;
        clazz->method_table->interface_implementations = mm_malloc(sizeof(struct it_INTERFACE_IMPLEMENTATION) * clazz->implemented_interfacesc);
        for(int j = 0; j < clazz->implemented_interfacesc; j++) {
            struct ts_TYPE_INTERFACE* interface = clazz->implemented_interfaces[j];
            struct it_INTERFACE_IMPLEMENTATION* interface_impl = &clazz->method_table->interface_implementations[j];
            interface_impl->interface_id = interface->id;
            interface_impl->method_table = mm_malloc(sizeof(struct it_METHOD_TABLE) + sizeof(struct it_METHOD*) * interface->methods->nmethods);
            for(int k = 0; k < interface->methods->nmethods; k++) {
                interface_impl->method_table->methods[k] = clazz->method_table->methods[cl_get_method_index(clazz->method_table, interface->methods->methods[k]->name)];
            }
        }
        // Bubble sort
        struct it_INTERFACE_IMPLEMENTATION* implementations = clazz->method_table->interface_implementations;
        bool swapped = true;
        while(swapped) {
            swapped = false;
            for(int i = 0; i < clazz->implemented_interfacesc - 1; i++) {
                if(implementations[i].interface_id > implementations[i + 1].interface_id) {
                    struct it_INTERFACE_IMPLEMENTATION tmp = implementations[i + 1];
                    implementations[i + 1] = implementations[i];
                    implementations[i] = tmp;
                    swapped = true;
                }
            }
        }
    }
}
void cl_arrange_method_tables(struct it_PROGRAM* program) {
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
        cl_arrange_method_tables_inner(program, visited, program->clazzes[i]);
    }
    cl_arrange_interface_methods(program);
    cl_arrange_interface_implementations(program);
    #if DEBUG
    printf("Ending cl_arrange_phi_tables\n");
    #endif
}
int cl_get_field_index(struct ts_TYPE_CLAZZ* clazz, char* name) {
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
int cl_get_method_index(struct it_METHOD_TABLE* method_table, char* name) {
    for(int i = 0; i < method_table->nmethods; i++) {
        if(strcmp(method_table->methods[i]->name, name) == 0) {
            return i;
        }
    }
    printf("Method: '%s'\n", name);
    fatal("Unable to find member method");
    return -1; // Unreachable
}
struct ts_TYPE_INTERFACE* cl_get_or_create_interface(char* name, struct it_PROGRAM* program) {
    union ts_TYPE* type = ts_get_type_optional(name);
    if(type != NULL) {
        if(type->barebones.category != ts_CATEGORY_INTERFACE) {
            printf("Name: '%s'\n", name);
            fatal("Duplicate name between interface and non-interface");
        }
        return (struct ts_TYPE_INTERFACE*) type;
    }
    struct ts_TYPE_INTERFACE* interface = mm_malloc(sizeof(struct ts_TYPE_INTERFACE));
    interface->category = ts_CATEGORY_INTERFACE;
    interface->heirarchy_len = 1;
    interface->heirarchy = mm_malloc(sizeof(union ts_TYPE*) * 1);
    interface->heirarchy[0] = (union ts_TYPE*) interface;
    interface->name = strdup(name);
    interface->id = ts_allocate_type_id();
    // This is allocated and arranged in cl_arrange_interface_methods(...)
    interface->methods = NULL;
    program->interfaces[program->interface_index++] = interface;
    ts_register_type((union ts_TYPE*) interface, interface->name);
    return interface;
}
bool cl_class_implements_interface(struct ts_TYPE_CLAZZ* clazz, struct ts_TYPE_INTERFACE* interface) {
    for(int i = 0; i < clazz->implemented_interfacesc; i++) {
        if(clazz->implemented_interfaces[i] == interface) {
            return true;
        }
    }
    return false;
}