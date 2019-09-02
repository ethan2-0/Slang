#include <common.h>
#include <interpreter.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

int cl_get_index(struct it_PROGRAM* program, struct ts_TYPE* type) {
    for(int i = 0; i < program->clazzesc; i++) {
        if(program->clazzes[i] == type) {
            return i;
        }
    }
    return -1;
}
void cl_arrange_method_tables_inner(struct it_PROGRAM* program, bool* visited, struct ts_TYPE* type) {
    #if DEBUG
    printf("Arranging phi tables for class %d\n", type->id);
    #endif
    int total_methods = 0;
    if(type->data.clazz.immediate_supertype != NULL) {
        if(!visited[cl_get_index(program, type->data.clazz.immediate_supertype)]) {
            cl_arrange_method_tables_inner(program, visited, type->data.clazz.immediate_supertype);
        }
        total_methods += type->data.clazz.immediate_supertype->data.clazz.method_table->nmethods;
    }
    for(int i = 0; i < program->methodc; i++) {
        if(program->methods[i].containing_clazz == type) {
            total_methods++;
        }
    }

    type->data.clazz.method_table = mm_malloc(sizeof(struct it_METHOD_TABLE) + sizeof(struct it_METHOD*) * total_methods);
    type->data.clazz.method_table->type = type;
    type->data.clazz.method_table->category = type->category;

    int current_method_index = 0;
    if(type->data.clazz.immediate_supertype != NULL) {
        memcpy(&type->data.clazz.method_table->methods, &type->data.clazz.immediate_supertype->data.clazz.method_table->methods, sizeof(struct it_METHOD*) * type->data.clazz.immediate_supertype->data.clazz.method_table->nmethods);
        current_method_index = type->data.clazz.immediate_supertype->data.clazz.method_table->nmethods;
    }
    for(int i = 0; i < program->methodc; i++) {
        if(program->methods[i].containing_clazz == type) {
            bool foundIt = false;
            if(type->data.clazz.immediate_supertype != NULL) {
                for(int j = 0; j < type->data.clazz.immediate_supertype->data.clazz.method_table->nmethods; j++) {
                    if(strcmp(type->data.clazz.method_table->methods[j]->name, program->methods[i].name) == 0) {
                        foundIt = true;
                        type->data.clazz.method_table->methods[j] = &program->methods[i];
                        break;
                    }
                }
            }
            if(!foundIt) {
                type->data.clazz.method_table->methods[current_method_index++] = &program->methods[i];
            }
        }
    }

    type->data.clazz.method_table->nmethods = current_method_index;

    if(type->data.clazz.immediate_supertype != NULL) {
        struct ts_CLAZZ_FIELD* old_fields = type->data.clazz.fields;
        type->data.clazz.fields = mm_malloc(sizeof(struct ts_CLAZZ_FIELD) * (type->data.clazz.nfields + type->data.clazz.immediate_supertype->data.clazz.nfields));
        memcpy(type->data.clazz.fields, type->data.clazz.immediate_supertype->data.clazz.fields, sizeof(struct ts_CLAZZ_FIELD) * type->data.clazz.immediate_supertype->data.clazz.nfields);
        memcpy(type->data.clazz.fields + type->data.clazz.immediate_supertype->data.clazz.nfields, old_fields, sizeof(struct ts_CLAZZ_FIELD) * type->data.clazz.nfields);
        free(old_fields);
        type->data.clazz.nfields += type->data.clazz.immediate_supertype->data.clazz.nfields;
        type->heirarchy_len = type->data.clazz.immediate_supertype->heirarchy_len + 1;
        // Yes, this is supposed to be size of a pointer
        free(type->heirarchy);
        type->heirarchy = malloc(sizeof(struct ts_TYPE*) * type->heirarchy_len);
        memcpy(type->heirarchy, type->data.clazz.immediate_supertype->heirarchy, sizeof(struct ts_TYPE*) * type->data.clazz.immediate_supertype->heirarchy_len);
        type->heirarchy[type->heirarchy_len - 1] = type;
    } else {
        free(type->heirarchy);
        type->heirarchy = malloc(sizeof(struct ts_TYPE*) * 1);
        type->heirarchy_len = 1;
        type->heirarchy[0] = type;
    }

    if(type->data.clazz.immediate_supertype != NULL) {
        int new_num_interfaces = type->data.clazz.implemented_interfacesc + type->data.clazz.immediate_supertype->data.clazz.implemented_interfacesc;
        type->data.clazz.implemented_interfaces = realloc(type->data.clazz.implemented_interfaces, sizeof(struct it_INTERFACE_IMPLEMENTATION) * new_num_interfaces);
        memcpy(type->data.clazz.implemented_interfaces + type->data.clazz.implemented_interfacesc, type->data.clazz.immediate_supertype->data.clazz.implemented_interfaces, sizeof(struct it_INTERFACE_IMPLEMENTATION) * type->data.clazz.immediate_supertype->data.clazz.implemented_interfacesc);
        type->data.clazz.implemented_interfacesc = new_num_interfaces;
    }

    int index = cl_get_index(program, type);
    visited[index] = true;
    #if DEBUG
    printf("Finished arranging phi tables for class %d\n", type->id);
    #endif
}
void cl_arrange_interface_methods(struct it_PROGRAM* program) {
    for(int i = 0; i < program->interfacesc; i++) {
        struct ts_TYPE* interface = program->interfaces[i];
        int num_methods = 0;
        for(int j = 0; j < program->methodc; j++) {
            if(program->methods[j].containing_interface == interface) {
                num_methods++;
            }
        }
        interface->data.interface.methods = mm_malloc(sizeof(struct it_METHOD_TABLE) + sizeof(struct it_METHOD*) * num_methods);
        interface->data.interface.methods->category = ts_CATEGORY_INTERFACE;
        interface->data.interface.methods->type = interface;
        interface->data.interface.methods->interface_implementations = NULL;
        interface->data.interface.methods->ninterface_implementations = 0;
        interface->data.interface.methods->nmethods = num_methods;
        int index = 0;
        for(int j = 0; j < program->methodc; j++) {
            if(program->methods[j].containing_interface == interface) {
                interface->data.interface.methods->methods[index++] = &program->methods[j];
            }
        }
    }
}
void cl_arrange_interface_implementations(struct it_PROGRAM* program) {
    for(int i = 0; i < program->clazzesc; i++) {
        struct ts_TYPE* clazz = program->clazzes[i];
        clazz->data.clazz.method_table->ninterface_implementations = clazz->data.clazz.implemented_interfacesc;
        clazz->data.clazz.method_table->interface_implementations = mm_malloc(sizeof(struct it_INTERFACE_IMPLEMENTATION) * clazz->data.clazz.implemented_interfacesc);
        for(int j = 0; j < clazz->data.clazz.implemented_interfacesc; j++) {
            struct ts_TYPE* interface = clazz->data.clazz.implemented_interfaces[j];
            struct it_INTERFACE_IMPLEMENTATION* interface_impl = &clazz->data.clazz.method_table->interface_implementations[j];
            interface_impl->interface_id = interface->id;
            interface_impl->method_table = mm_malloc(sizeof(struct it_METHOD_TABLE) + sizeof(struct it_METHOD*) * interface->data.interface.methods->nmethods);
            for(int k = 0; k < interface->data.interface.methods->nmethods; k++) {
                interface_impl->method_table->methods[k] = clazz->data.clazz.method_table->methods[cl_get_method_index(clazz->data.clazz.method_table, interface->data.interface.methods->methods[k]->name)];
            }
        }
        // Bubble sort
        struct it_INTERFACE_IMPLEMENTATION* implementations = clazz->data.clazz.method_table->interface_implementations;
        bool swapped = true;
        while(swapped) {
            swapped = false;
            for(int i = 0; i < clazz->data.clazz.implemented_interfacesc - 1; i++) {
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
    printf("Entering cl_arrange_method_tables\n");
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
    printf("Ending cl_arrange_method_tables\n");
    #endif
}
int cl_get_field_index(struct ts_TYPE* clazz, char* name) {
    for(int i = 0; i < clazz->data.clazz.nfields; i++) {
        if(strcmp(clazz->data.clazz.fields[i].name, name) == 0) {
            return i;
        }
    }
    #if DEBUG
    printf("Field name: '%s'\n", name);
    #endif
    fatal("Unable to find field");
    return -1; // Unreachable
}
int cl_get_method_index_optional(struct it_METHOD_TABLE* method_table, char* name) {
    for(int i = 0; i < method_table->nmethods; i++) {
        if(strcmp(method_table->methods[i]->name, name) == 0) {
            return i;
        }
    }
    return -1;
}
int cl_get_method_index(struct it_METHOD_TABLE* method_table, char* name) {
    int index = cl_get_method_index_optional(method_table, name);
    if(index < 0) {
        printf("Method: '%s'\n", name);
        fatal("Unable to find member method");
        return -1; // Unreachable
    }
    return index;
}
struct ts_TYPE* cl_get_or_create_interface(char* name, struct it_PROGRAM* program) {
    struct ts_TYPE* type = ts_get_type_optional(name, NULL);
    if(type != NULL) {
        if(type->category != ts_CATEGORY_INTERFACE) {
            printf("Name: '%s'\n", name);
            fatal("Duplicate name between interface and non-interface");
        }
        return type;
    }
    struct ts_TYPE* interface = mm_malloc(sizeof(struct ts_TYPE));
    interface->category = ts_CATEGORY_INTERFACE;
    interface->heirarchy_len = 1;
    interface->heirarchy = mm_malloc(sizeof(struct ts_TYPE*) * 1);
    interface->heirarchy[0] = interface;
    interface->name = strdup(name);
    interface->id = ts_allocate_type_id();
    // This is allocated and arranged in cl_arrange_interface_methods(...)
    interface->data.interface.methods = NULL;
    program->interfaces[program->interface_index++] = interface;
    ts_register_type(interface, interface->name);
    return interface;
}
bool cl_class_implements_interface(struct ts_TYPE* clazz, struct ts_TYPE* interface) {
    for(int i = 0; i < clazz->data.clazz.implemented_interfacesc; i++) {
        if(clazz->data.clazz.implemented_interfaces[i] == interface) {
            return true;
        }
    }
    return false;
}