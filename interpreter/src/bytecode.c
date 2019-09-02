#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "common.h"
#include "opcodes.h"
#include "interpreter.h"
#include "slbstdlib.h"

uint32_t bc_resolve_name(struct it_PROGRAM* program, char* callee_name);
uint32_t bc_assign_method_id(struct it_PROGRAM* program);

void bc_parse_opcode(struct fr_STATE* state, struct it_PROGRAM* program, struct it_METHOD* method, struct it_OPCODE* opcode) {
    uint8_t opcode_num = fr_getuint8(state);
    opcode->type = opcode_num;
    #if DEBUG
    printf("Opcode %02x\n", opcode_num);
    #endif
    if(opcode_num == OPCODE_LOAD) {
        struct it_OPCODE_DATA_LOAD* data = &opcode->data.load;
        data->target = fr_getuint32(state);
        data->data = fr_getuint64(state);
    } else if(opcode_num == OPCODE_ZERO) {
        struct it_OPCODE_DATA_ZERO* data = &opcode->data.zero;
        data->target = fr_getuint32(state);
    } else if(opcode_num == OPCODE_ADD) {
        struct it_OPCODE_DATA_ADD* data = &opcode->data.add;
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
    } else if(opcode_num == OPCODE_TWOCOMP) {
        struct it_OPCODE_DATA_TWOCOMP* data = &opcode->data.twocomp;
        data->target = fr_getuint32(state);
    } else if(opcode_num == OPCODE_MULT) {
        struct it_OPCODE_DATA_MULT* data = &opcode->data.mult;
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
    } else if(opcode_num == OPCODE_MODULO) {
        struct it_OPCODE_DATA_MODULO* data = &opcode->data.modulo;
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
    } else if(opcode_num == OPCODE_CALL) {
        struct it_OPCODE_DATA_CALL* data = &opcode->data.call;
        char* callee_name = fr_getstr(state);
        data->callee = &program->methods[bc_resolve_name(program, callee_name)];
        free(callee_name);
        data->returnval = fr_getuint32(state);
        memset(data->type_params, 0, sizeof(data->type_params));
        uint32_t num_types = fr_getuint32(state);
        if(num_types > TS_MAX_TYPE_ARGS) {
            fatal("Maximum number of type arguments exceeded");
        }
        data->type_paramsc = num_types;
        for(uint32_t i = 0; i < num_types; i++) {
            data->type_params[i] = fr_getuint32(state);
        }
    } else if(opcode_num == OPCODE_RETURN) {
        struct it_OPCODE_DATA_RETURN* data = &opcode->data.return_;
        data->target = fr_getuint32(state);
    } else if(opcode_num == OPCODE_EQUALS) {
        struct it_OPCODE_DATA_EQUALS* data = &opcode->data.equals;
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
    } else if(opcode_num == OPCODE_INVERT) {
        struct it_OPCODE_DATA_INVERT* data = &opcode->data.invert;
        data->target = fr_getuint32(state);
    } else if(opcode_num == OPCODE_LTEQ) {
        struct it_OPCODE_DATA_LTEQ* data = &opcode->data.lteq;
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
    } else if(opcode_num == OPCODE_GT) {
        struct it_OPCODE_DATA_GT* data = &opcode->data.gt;
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
    } else if(opcode_num == OPCODE_GOTO) {
        struct it_OPCODE_DATA_GOTO* data = &opcode->data.goto_;
        data->target = fr_getuint32(state);
    } else if(opcode_num == OPCODE_JF) {
        struct it_OPCODE_DATA_JF* data = &opcode->data.jf;
        data->predicate = fr_getuint32(state);
        data->target = fr_getuint32(state);
    } else if(opcode_num == OPCODE_PARAM) {
        struct it_OPCODE_DATA_PARAM* data = &opcode->data.param;
        data->source = fr_getuint32(state);
        data->target = fr_getuint8(state);
    } else if(opcode_num == OPCODE_GTEQ) {
        struct it_OPCODE_DATA_GTEQ* data = &opcode->data.gteq;
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
    } else if(opcode_num == OPCODE_XOR) {
        struct it_OPCODE_DATA_XOR* data = &opcode->data.xor;
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
    } else if(opcode_num == OPCODE_AND) {
        struct it_OPCODE_DATA_AND* data = &opcode->data.and;
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
    } else if(opcode_num == OPCODE_OR) {
        struct it_OPCODE_DATA_OR* data = &opcode->data.or;
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
    } else if(opcode_num == OPCODE_MOV) {
        struct it_OPCODE_DATA_MOV* data = &opcode->data.mov;
        data->source = fr_getuint32(state);
        data->target = fr_getuint32(state);
    } else if(opcode_num == OPCODE_NOP) {
        // Do nothing
        ;
    } else if(opcode_num == OPCODE_LT) {
        struct it_OPCODE_DATA_LT* data = &opcode->data.lt;
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
    } else if(opcode_num == OPCODE_NEW) {
        struct it_OPCODE_DATA_NEW* data = &opcode->data.new;
        data->clazz = ts_get_type(fr_getstr(state), method->type_parameters);
        if(data->clazz->category != ts_CATEGORY_CLAZZ) {
            fatal("Attempt to instantiate something that isn't a class");
        }
        data->dest = fr_getuint32(state);
    } else if(opcode_num == OPCODE_ACCESS) {
        struct it_OPCODE_DATA_ACCESS* data = &opcode->data.access;
        data->clazzreg = fr_getuint32(state);
        struct ts_TYPE* clazzreg_type = method->register_types[data->clazzreg];
        if(clazzreg_type->category != ts_CATEGORY_CLAZZ) {
            fatal("Attempt to access something that isn't a class");
        }
        data->property_index = cl_get_field_index(clazzreg_type, fr_getstr(state));
        data->destination = fr_getuint32(state);
    } else if(opcode_num == OPCODE_ASSIGN) {
        struct it_OPCODE_DATA_ASSIGN* data = &opcode->data.assign;
        data->clazzreg = fr_getuint32(state);
        struct ts_TYPE* clazzreg_type = method->register_types[data->clazzreg];
        if(clazzreg_type->category != ts_CATEGORY_CLAZZ) {
            fatal("Attempt to assign to a property of something that isn't a class");
        }
        data->property_index = cl_get_field_index(clazzreg_type, fr_getstr(state));
        data->source = fr_getuint32(state);
    } else if(opcode_num == OPCODE_CLASSCALL) {
        struct it_OPCODE_DATA_CLASSCALL* data = &opcode->data.classcall;
        data->targetreg = fr_getuint32(state);
        struct ts_TYPE* clazzreg_type = method->register_types[data->targetreg];
        if(clazzreg_type->category != ts_CATEGORY_CLAZZ && clazzreg_type->category != ts_CATEGORY_TYPE_PARAMETER) {
            fatal("Attempt to call a method of something that isn't a class");
        }
        char* callee_name = fr_getstr(state);
        struct it_METHOD_TABLE* method_table;
        if(clazzreg_type->category == ts_CATEGORY_CLAZZ) {
            method_table = clazzreg_type->data.clazz.method_table;
        } else if(clazzreg_type->category == ts_CATEGORY_TYPE_PARAMETER) {
            if(clazzreg_type->data.type_parameter.extends == NULL) {
                fatal("Attempt to call a method of a type parameter that doesn't extend anything");
            }
            method_table = clazzreg_type->data.type_parameter.extends->data.clazz.method_table;
        }
        data->callee_index = cl_get_method_index(method_table, callee_name);
        free(callee_name);
        data->returnreg = fr_getuint32(state);
    } else if(opcode_num == OPCODE_ARRALLOC) {
        struct it_OPCODE_DATA_ARRALLOC* data = &opcode->data.arralloc;
        data->arrreg = fr_getuint32(state);
        data->lengthreg = fr_getuint32(state);
    } else if(opcode_num == OPCODE_ARRACCESS) {
        struct it_OPCODE_DATA_ARRACCESS* data = &opcode->data.arraccess;
        data->arrreg = fr_getuint32(state);
        data->indexreg = fr_getuint32(state);
        data->elementreg = fr_getuint32(state);
    } else if(opcode_num == OPCODE_ARRASSIGN) {
        struct it_OPCODE_DATA_ARRASSIGN* data = &opcode->data.arrassign;
        data->arrreg = fr_getuint32(state);
        data->indexreg = fr_getuint32(state);
        data->elementreg = fr_getuint32(state);
    } else if(opcode_num == OPCODE_ARRLEN) {
        struct it_OPCODE_DATA_ARRLEN* data = &opcode->data.arrlen;
        data->arrreg = fr_getuint32(state);
        data->resultreg = fr_getuint32(state);
    } else if(opcode_num == OPCODE_DIV) {
        struct it_OPCODE_DATA_DIV* data = &opcode->data.div;
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
    } else if(opcode_num == OPCODE_CAST) {
        struct it_OPCODE_DATA_CAST* data = &opcode->data.cast;
        data->source = fr_getuint32(state);
        data->target = fr_getuint32(state);
        struct ts_TYPE* target_type = method->register_types[data->target];
        if(target_type->category != ts_CATEGORY_CLAZZ && target_type->category != ts_CATEGORY_INTERFACE) {
            fatal("Cannot cast to a type that isn't a class or interface. This is a compiler bug.");
        }
    } else if(opcode_num == OPCODE_INSTANCEOF) {
        struct it_OPCODE_DATA_INSTANCEOF* data = &opcode->data.instanceof;
        data->source = fr_getuint32(state);
        data->destination = fr_getuint32(state);
        data->predicate_type_index = fr_getuint32(state);
    } else if(opcode_num == OPCODE_STATICVARGET) {
        struct it_OPCODE_DATA_STATICVARGET* data = &opcode->data.staticvarget;
        char* source_var_name = fr_getstr(state);
        data->source_var = sv_get_static_var_index_by_name(program, source_var_name);
        data->destination = fr_getuint32(state);
        free(source_var_name);
    } else if(opcode_num == OPCODE_STATICVARSET) {
        struct it_OPCODE_DATA_STATICVARSET* data = &opcode->data.staticvarset;
        data->source = fr_getuint32(state);
        char* dest_var_name = fr_getstr(state);
        data->destination_var = sv_get_static_var_index_by_name(program, dest_var_name);
        free(dest_var_name);
    } else if(opcode_num == OPCODE_CLASSCALLSPECIAL) {
        struct it_OPCODE_DATA_CLASSCALLSPECIAL* data = &opcode->data.classcallspecial;
        char* classname = fr_getstr(state);
        char* methodname = fr_getstr(state);
        data->destination_register = fr_getuint32(state);
        data->callee_register = fr_getuint32(state);
        struct ts_TYPE* clazz = ts_get_type(classname, method->type_parameters);
        // This is safe since every member of union ts_TYPE has offsetof(category) the same
        if(clazz->category != ts_CATEGORY_CLAZZ) {
            fatal("First argument to CLASSCALLSPECIAL isn't a class");
        }
        uint32_t method_index = cl_get_method_index(clazz->data.clazz.method_table, methodname);
        data->method = clazz->data.clazz.method_table->methods[method_index];
        free(classname);
        free(methodname);
    } else if(opcode_num == OPCODE_INTERFACECALL) {
        struct it_OPCODE_DATA_INTERFACECALL* data = &opcode->data.interfacecall;
        data->callee_register = fr_getuint32(state);
        char* method_name = fr_getstr(state);
        struct ts_TYPE* interface = method->register_types[data->callee_register];
        if(interface->category == ts_CATEGORY_INTERFACE) {
            data->interface_id = interface->id;
            data->method_index = cl_get_method_index(interface->data.interface.methods, method_name);
        } else if(interface->category == ts_CATEGORY_TYPE_PARAMETER) {
            data->method_index = -1;
            for(int i = 0; i < interface->data.type_parameter.implementsc; i++) {
                data->method_index = cl_get_method_index_optional(interface->data.type_parameter.implements[i]->data.interface.methods, method_name);
                if(data->method_index != -1) {
                    data->interface_id = interface->data.type_parameter.implements[i]->id;
                    break;
                }
            }
            if(data->method_index == -1) {
                printf("Method name: %s\n", method_name);
                fatal("Could not find interface method for type parameter");
            }
        } else {
            fatal("Attempt to do interface call on something that isn't an interface or type parameter");
        }
        data->destination_register = fr_getuint32(state);
    } else {
        printf("Opcode: %2x\n", opcode_num);
        fatal("Unrecognized opcode");
    }
}
struct bc_PRESCAN_RESULTS {
    int num_methods;
    uint32_t entrypoint_id;
    int num_clazzes;
    int num_static_variables;
};
struct bc_PRESCAN_RESULTS* bc_prescan(struct fr_STATE* state) {
    #if DEBUG
    printf("Entering bc_prescan\n");
    #endif
    struct bc_PRESCAN_RESULTS* results = mm_malloc(sizeof(struct bc_PRESCAN_RESULTS));
    results->num_methods = 0;
    results->num_clazzes = 0;
    results->num_static_variables = 0;

    if(fr_getuint32(state) != 0xcf702b56) {
        fatal("Incorrect bytecode file magic number");
    }

    while(!fr_iseof(state)) {
        uint8_t segment_type = fr_getuint8(state);
        if(segment_type == SEGMENT_TYPE_METHOD) {
            // Length + header + body
            // header = ID + nargs + len(segment.signature.name) + segment.signature.name
            uint32_t length = fr_getuint32(state);
            fr_advance(state, (int) length);
            results->num_methods++;
        } else if(segment_type == SEGMENT_TYPE_METADATA) {
            // results->entrypoint_id = fr_getuint32(state);
            // Ignore the entrypoint name at this step of parsing
            free(fr_getstr(state));
            free(fr_getstr(state));
        } else if(segment_type == SEGMENT_TYPE_CLASS) {
            uint32_t length = fr_getuint32(state);
            fr_advance(state, (int) length);
            results->num_clazzes++;
        } else if(segment_type == SEGMENT_TYPE_STATIC_VARIABLES) {
            uint32_t length = fr_getuint32(state);
            results->num_static_variables = fr_getuint32(state);
            fr_advance(state, (int) length - 4);
        } else if(segment_type == SEGMENT_TYPE_INTERFACE) {
            free(fr_getstr(state));
        } else {
            fatal("Unrecognized segment type");
        }
    }

    fr_rewind(state);
    #if DEBUG
    printf("Exiting bc_prescan\n");
    #endif
    return results;
}
union itval bc_create_staticvar_value(struct it_PROGRAM* program, struct fr_STATE* state, struct ts_TYPE* type) {
    if(type->category == ts_CATEGORY_PRIMITIVE) {
        union itval ret;
        ret.number = fr_getuint64(state);
        return ret;
    } else if(type->category == ts_CATEGORY_ARRAY) {
        uint64_t length = fr_getuint64(state);
        size_t allocation_size = sizeof(struct it_ARRAY_DATA) + (length == 0 ? 0 : (length - 1) * sizeof(union itval));
        struct it_ARRAY_DATA* array_data = mm_malloc(allocation_size);
        array_data->length = length;
        array_data->type = type;
        for(uint64_t i = 0; i < length; i++) {
            array_data->elements[i] = bc_create_staticvar_value(program, state, type->data.array.parent_type);
        }
        union itval ret;
        ret.array_data = array_data;
        ret.array_data->gc_registry_entry = gc_register_object(ret, allocation_size, ts_CATEGORY_ARRAY);
        return ret;
    } else if(type->category == ts_CATEGORY_CLAZZ) {
        union itval ret;
        ret.clazz_data = NULL;
        fr_getuint64(state);
        return ret;
    } else {
        fatal("Unrecognized category in bc_create_staticvar_value(...)");
    }
    union itval ret;
    ret.number = 0;
    return ret; // Unreachable
}
void bc_scan_static_vars(struct it_PROGRAM* program, struct fr_STATE* state) {
    while(!fr_iseof(state)) {
        uint8_t segment_type = fr_getuint8(state);
        if(segment_type == SEGMENT_TYPE_METHOD) {
            uint32_t length = fr_getuint32(state);
            fr_advance(state, (int) length);
        } else if(segment_type == SEGMENT_TYPE_METADATA) {
            free(fr_getstr(state));
            free(fr_getstr(state));
        } else if(segment_type == SEGMENT_TYPE_CLASS) {
            uint32_t length = fr_getuint32(state);
            fr_advance(state, (int) length);
        } else if(segment_type == SEGMENT_TYPE_STATIC_VARIABLES) {
            fr_getuint32(state); // Length
            uint32_t num_static_vars = fr_getuint32(state);
            for(int i = 0; i < num_static_vars; i++) {
                char* name = fr_getstr(state);
                char* typename = fr_getstr(state);
                struct ts_TYPE* type = ts_get_type(typename, NULL);
                union itval value = bc_create_staticvar_value(program, state, type);
                sv_add_static_var(program, type, strdup(name), value);
                free(name);
                free(typename);
            }
        } else if(segment_type == SEGMENT_TYPE_INTERFACE) {
            free(fr_getstr(state));
        } else {
            fatal("Unrecognized segment type");
        }
    }
}
uint32_t bc_assign_method_id(struct it_PROGRAM* program) {
    return program->method_id++;
}
void bc_prescan_destroy(struct bc_PRESCAN_RESULTS* prescan) {
    free(prescan);
}
uint32_t bc_resolve_name(struct it_PROGRAM* program, char* name) {
    for(int i = 0; i < program->methodc; i++) {
        if(strcmp(name, program->methods[i].name) == 0) {
            #if DEBUG
            printf("Resolved %s to %d\n", name, program->methods[i].id);
            #endif
            return program->methods[i].id;
        }
    }
    #if DEBUG
    printf("Name: %s\n", name);
    #endif
    printf("Name: '%s'\n", name);
    fatal("Asked to resolve unknown name");
    // Unreachable
    return -1;
}
void bc_parse_method(struct fr_STATE* state, struct it_OPCODE* opcode_buff, struct it_PROGRAM* program, struct it_METHOD* result) {
    result->replacement_ptr = NULL;
    fr_getuint32(state); // length
    uint32_t registerc = fr_getuint32(state);
    uint32_t nargs = fr_getuint32(state);
    uint32_t opcodec = fr_getuint32(state);
    char* name = fr_getstr(state);
    uint32_t id = bc_resolve_name(program, name);

    #if DEBUG
    printf("Register count %d\n", registerc);
    printf("Id %d\n", id);
    printf("Nargs %d\n", nargs);
    #endif

    free(fr_getstr(state)); // Containing class/interface

    uint32_t num_type_arguments = fr_getuint32(state);
    result->type_parameters = ts_create_generic_type_context(num_type_arguments, NULL);
    for(uint32_t i = 0; i < num_type_arguments; i++) {
        char* type_parameter_name = fr_getstr(state);
        char* extends_name = fr_getstr(state);
        // Our generic type context isn't fully initialized yet, so don't pass
        // it as an argument here.
        struct ts_TYPE* extends = strlen(extends_name) > 0 ? ts_get_type(extends_name, NULL) : NULL;
        free(extends_name);
        int implementsc = fr_getuint32(state);
        struct ts_TYPE* implements[implementsc];
        for(int j = 0; j < implementsc; j++) {
            char* implements_name = fr_getstr(state);
            implements[j] = ts_get_type(implements_name, NULL);
            free(implements_name);
        }
        // Passing in a stack-allocated array since ts_init_type_parameter(...) just memcpy's it.
        ts_init_type_parameter(&result->type_parameters->arguments[i], type_parameter_name, result->type_parameters, extends, implementsc, (struct ts_TYPE**) &implements);
    }

    result->returntype = ts_get_type(fr_getstr(state), result->type_parameters);
    result->register_types = mm_malloc(sizeof(struct ts_TYPE*) * registerc);
    for(int i = 0; i < registerc; i++) {
        char* typename = fr_getstr(state);
        #if DEBUG
        printf("Got register type '%s'\n", typename);
        #endif
        result->register_types[i] = ts_get_type(typename, result->type_parameters);
    }

    result->typereferencec = fr_getuint32(state);
    result->typereferences = mm_malloc(sizeof(struct ts_TYPE*) * result->typereferencec);
    for(uint32_t i = 0; i < result->typereferencec; i++) {
        char* typename = fr_getstr(state);
        result->typereferences[i] = ts_get_type(typename, result->type_parameters);
        free(typename);
    }

    for(int i = 0; i < opcodec; i++) {
        bc_parse_opcode(state, program, result, &opcode_buff[i]);
    }
    for(int i = 0; i < opcodec; i++) {
        opcode_buff[i].linenum = fr_getuint32(state);
    }

    result->registerc = registerc;
    result->id = id;
    result->nargs = nargs;
    result->opcodec = opcodec;

    #if DEBUG
    printf("Opcodec: %d\n", result->opcodec);
    #endif

    result->opcodes = mm_malloc(sizeof(struct it_OPCODE) * opcodec);

    #if DEBUG
    for(int i = 0; i < opcodec; i++) {
        printf("Opcode--: %02x\n", opcode_buff[i].type);
    }
    #endif

    memcpy(result->opcodes, opcode_buff, sizeof(struct it_OPCODE) * opcodec);

    #if DEBUG
    printf("Done parsing method\n");
    #endif
}
void bc_scan_types_firstpass(struct it_PROGRAM* program, struct bc_PRESCAN_RESULTS* prescan, struct fr_STATE* state) {
    while(!fr_iseof(state)) {
        uint8_t segment_type = fr_getuint8(state);
        if(segment_type == SEGMENT_TYPE_METHOD) {
            fr_advance(state, fr_getuint32(state));
        } else if(segment_type == SEGMENT_TYPE_STATIC_VARIABLES) {
            fr_advance(state, fr_getuint32(state));
        } else if(segment_type == SEGMENT_TYPE_METADATA) {
            free(fr_getstr(state));
            free(fr_getstr(state));
        } else if(segment_type == SEGMENT_TYPE_CLASS) {
            fr_getuint32(state); // length
            char* clazzname = fr_getstr(state);
            char* parentname = fr_getstr(state);
            uint32_t num_interfaces = fr_getuint32(state);
            for(uint32_t i = 0; i < num_interfaces; i++) {
                free(fr_getstr(state));
            }
            #if DEBUG
            printf("Prescanning class '%s'\n", clazzname);
            #endif
            uint32_t numfields = fr_getuint32(state);
            for(uint32_t i = 0; i < numfields; i++) {
                free(fr_getstr(state));
                free(fr_getstr(state));
            }
            struct ts_TYPE* clazz = mm_malloc(sizeof(struct ts_TYPE));
            clazz->category = ts_CATEGORY_CLAZZ;
            clazz->id = ts_allocate_type_id();
            clazz->name = strdup(clazzname);
            clazz->heirarchy_len = 1;
            clazz->heirarchy = mm_malloc(sizeof(struct ts_TYPE*) * 1);
            clazz->heirarchy[0] = clazz;
            clazz->data.clazz.nfields = -1;
            clazz->data.clazz.fields = NULL;
            clazz->data.clazz.implemented_interfaces = mm_malloc(sizeof(struct ts_TYPE*) * num_interfaces);
            clazz->data.clazz.implemented_interfacesc = num_interfaces;
            // Check for duplicate class names
            for(int i = 0; i < program->clazz_index; i++) {
                if(strcmp(program->clazzes[i]->name, clazz->name) == 0) {
                    printf("Duplicate class name: '%s'\n", clazz->name);
                    fatal("Duplicate class name");
                }
            }
            ts_register_type(clazz, strdup(clazzname));
            program->clazzes[program->clazz_index++] = clazz;
            free(clazzname);
            free(parentname);
        } else if(segment_type == SEGMENT_TYPE_INTERFACE) {
            free(fr_getstr(state));
            program->interfacesc++;
        } else {
            fatal("Unrecognized segment type");
        }
    }
    program->interfaces = mm_malloc(sizeof(struct ts_TYPE*) * program->interfacesc);
}
void bc_scan_types_secondpass(struct it_PROGRAM* program, struct bc_PRESCAN_RESULTS* prescan, struct fr_STATE* state) {
    while(!fr_iseof(state)) {
        uint8_t segment_type = fr_getuint8(state);
        if(segment_type == SEGMENT_TYPE_METHOD) {
            fr_advance(state, fr_getuint32(state));
        } else if(segment_type == SEGMENT_TYPE_STATIC_VARIABLES) {
            fr_advance(state, fr_getuint32(state));
        } else if(segment_type == SEGMENT_TYPE_METADATA) {
            free(fr_getstr(state));
            free(fr_getstr(state));
        } else if(segment_type == SEGMENT_TYPE_CLASS) {
            fr_getuint32(state);
            char* clazzname = fr_getstr(state);
            char* supertype_name = fr_getstr(state);
            struct ts_TYPE* clazz = ts_get_type(clazzname, NULL);
            if(clazz->category != ts_CATEGORY_CLAZZ) {
                fatal("Tried to extend something that isn't a class");
            }
            if(strlen(supertype_name) > 0) {
                clazz->data.clazz.immediate_supertype = ts_get_type(supertype_name, NULL);
                if(clazz->data.clazz.immediate_supertype == NULL) {
                    fatal("Couldn't find superclass");
                }
                if(clazz->data.clazz.immediate_supertype->category != ts_CATEGORY_CLAZZ) {
                    fatal("Tried to extend something that isn't a class");
                }
            } else {
                clazz->data.clazz.immediate_supertype = NULL;
            }
            uint32_t num_interfaces = fr_getuint32(state);
            for(uint32_t i = 0; i < num_interfaces; i++) {
                char* name = fr_getstr(state);
                struct ts_TYPE* interface = cl_get_or_create_interface(name, program);
                clazz->data.clazz.implemented_interfaces[i] = interface;
                free(name);
            }
            uint32_t numfields = fr_getuint32(state);
            clazz->data.clazz.nfields = numfields;
            clazz->data.clazz.fields = mm_malloc(sizeof(struct ts_CLAZZ_FIELD) * numfields);
            for(uint32_t i = 0; i < numfields; i++) {
                clazz->data.clazz.fields[i].name = fr_getstr(state);
                clazz->data.clazz.fields[i].type = ts_get_type(fr_getstr(state), NULL);
            }
            #if DEBUG
            printf("Scanning class '%s'\n", clazzname);
            #endif
            free(supertype_name);
            free(clazzname);
        } else if(segment_type == SEGMENT_TYPE_INTERFACE) {
            char* name = fr_getstr(state);
            cl_get_or_create_interface(name, program);
            free(name);
        } else {
            fatal("Unrecognized segment type");
        }
    }
}
void bc_scan_methods(struct it_PROGRAM* program, struct fr_STATE* state, int offset) {
    // state->methodc and state->methods are already initialized
    #if DEBUG
    printf("Entering bc_scan_methods()\n");
    #endif
    int methodindex = offset;
    while(!fr_iseof(state)) {
        uint8_t segment_type = fr_getuint8(state);
        if(segment_type == SEGMENT_TYPE_METHOD) {
            #if DEBUG
            printf("Method\n");
            #endif
            uint32_t length = fr_getuint32(state);
            uint32_t end_index = state->index + length;
            struct it_METHOD* method = &program->methods[methodindex++];
            method->registerc = fr_getuint32(state); // num_registers
            method->id = bc_assign_method_id(program); // id
            method->nargs = fr_getuint32(state); // nargs
            fr_getuint32(state);
            method->name = fr_getstr(state); // name
            char* containing_clazz_name = fr_getstr(state);
            method->containing_clazz = NULL;
            method->containing_interface = NULL;
            method->typereferencec = 0;
            method->typereferences = NULL;
            method->reificationsc = 0;
            method->reifications = NULL;
            method->reifications_size = 0;
            method->has_had_references_reified = false;
            method->typeargs = NULL;
            if(strlen(containing_clazz_name) > 0) {
                struct ts_TYPE* containing_type = ts_get_type(containing_clazz_name, NULL);
                if(containing_type->category == ts_CATEGORY_CLAZZ) {
                    method->containing_clazz = containing_type;
                } else if(containing_type->category == ts_CATEGORY_INTERFACE) {
                    method->containing_interface = containing_type;
                } else {
                    fatal("Method is contained in a type that isn't a class or an interface");
                }
            }
            free(containing_clazz_name);
            // Make sure there are no duplicate method names
            for(int i = 0; i < methodindex - 1; i++) {
                if(strcmp(method->name, program->methods[i].name) == 0 && method->containing_clazz == program->methods[i].containing_clazz) {
                    printf("Duplicate method name and containing class: '%s'", method->name);
                    if(method->containing_clazz != NULL) {
                        printf(" from class '%s'\n", method->containing_clazz->name);
                    } else {
                        printf(" with no containing class\n");
                    }
                    fatal("Duplicate method name");
                }
            }
            #if DEBUG
            printf("Method name is %s\n", method->name);
            #endif
            fr_advance(state, end_index - state->index);
        } else if(segment_type == SEGMENT_TYPE_METADATA) {
            #if DEBUG
            printf("Metadata\n");
            #endif
            char* name = fr_getstr(state);
            // A zero-length entrypoint name means no entrypoint
            if(strlen(name) > 0) {
                program->entrypoint = bc_resolve_name(program, name);
            }
            free(fr_getstr(state));
        } else if(segment_type == SEGMENT_TYPE_STATIC_VARIABLES) {
            uint32_t length = fr_getuint32(state);
            fr_advance(state, (int) length);
        } else if(segment_type == SEGMENT_TYPE_CLASS) {
            uint32_t length = fr_getuint32(state);
            fr_advance(state, (int) length);
        } else if(segment_type == SEGMENT_TYPE_INTERFACE) {
            free(fr_getstr(state));
        } else {
            fatal("Unrecognized segment type");
        }
    }
    #if DEBUG
    printf("Exiting bc_scan_methods()\n");
    #endif
}
struct it_PROGRAM* bc_parse_from_files(int fpc, FILE* fp[], struct it_OPTIONS* options) {
    #if DEBUG
    printf("Parsing %d files\n", fpc);
    #endif
    int num_files = fpc + 1;
    struct fr_STATE* state[num_files];
    for(int i = 0; i < fpc; i++) {
        state[i] = fr_new(fp[i]);
    }
    state[fpc] = fr_new_from_buffer((size_t) (sl_stdlib_end - sl_stdlib), sl_stdlib);
    // This is the index of the start of the segments in the input bytecode
    struct bc_PRESCAN_RESULTS* prescan[num_files];
    struct it_PROGRAM* result = mm_malloc(sizeof(struct it_PROGRAM));
    result->clazz_index = 0;
    result->method_id = 0;
    result->methodc = 0;
    result->entrypoint = -1;
    result->static_varsc = 0;
    result->interfacesc = 0;
    result->interface_index = 0;
    for(int i = 0; i < num_files; i++) {
        prescan[i] = bc_prescan(state[i]);
        fr_rewind(state[i]);
        // Ignore the magic number
        fr_getuint32(state[i]);
        result->methodc += prescan[i]->num_methods;
        result->static_varsc += prescan[i]->num_static_variables;
    }
    result->clazzesc = 0;
    for(int i = 0; i < num_files; i++) {
        result->clazzesc += prescan[i]->num_clazzes;
    }
    result->static_vars = mm_malloc(sizeof(struct sv_STATIC_VAR) * result->static_varsc);
    #if DEBUG
    printf("%d methods\n", result->methodc);
    #endif
    result->methods = mm_malloc(sizeof(struct it_METHOD) * result->methodc);
    result->clazzes = mm_malloc(sizeof(struct ts_TYPE*) * result->clazzesc);
    for(int i = 0; i < num_files; i++) {
        bc_scan_types_firstpass(result, prescan[i], state[i]);
        fr_rewind(state[i]);
        // Ignore the magic number
        fr_getuint32(state[i]);
    }
    for(int i = 0; i < num_files; i++) {
        bc_scan_types_secondpass(result, prescan[i], state[i]);
        fr_rewind(state[i]);
        // Ignore the magic number
        fr_getuint32(state[i]);
    }
    // Trust me, this is correct, though why that is isn't obvious.
    for(int i = 0; i < result->interfacesc; i++) {
        if(result->interfaces[i] == NULL) {
            fatal("Duplicate interface name");
        }
    }
    #if DEBUG
    printf("Again, %d methods\n", result->methodc);
    #endif
    for(int i = 0, method_index = 0; i < num_files; i++) {
        bc_scan_methods(result, state[i], method_index);
        method_index += prescan[i]->num_methods;
        fr_rewind(state[i]);
        // Ignore the magic number
        fr_getuint32(state[i]);
    }
    for(int i = 0; i < num_files; i++) {
        bc_scan_static_vars(result, state[i]);
        fr_rewind(state[i]);
        // Ignore the magic number
        fr_getuint32(state[i]);
    }
    cl_arrange_method_tables(result);

    if(result->entrypoint == -1) {
        fatal("No entrypoint found");
    }

    // Reset the index into the input bytecode to the beginning of the segments
    int method_index = 0;
    int largest_bufflen = 0;
    for(int i = 0; i < num_files; i++) {
        if(state[i]->bufflen > largest_bufflen) {
            largest_bufflen = state[i]->bufflen;
        }
    }
    struct it_OPCODE* opcodes = mm_malloc(sizeof(struct it_OPCODE) * largest_bufflen);
    for(int i = 0; i < num_files; i++) {
        while(!fr_iseof(state[i])) {
            uint8_t segment_type = fr_getuint8(state[i]);
            if(segment_type == SEGMENT_TYPE_METHOD) {
                bc_parse_method(state[i], opcodes, result, &result->methods[method_index++]);
            } else if(segment_type == SEGMENT_TYPE_METADATA) {
                // Metadata segment
                free(fr_getstr(state[i]));
                free(fr_getstr(state[i]));
            } else if(segment_type == SEGMENT_TYPE_CLASS) {
                uint32_t length = fr_getuint32(state[i]);
                fr_advance(state[i], (int) length);
            } else if(segment_type == SEGMENT_TYPE_STATIC_VARIABLES) {
                fr_advance(state[i], fr_getuint32(state[i]));
            } else if(segment_type == SEGMENT_TYPE_INTERFACE) {
                free(fr_getstr(state[i]));
            } else {
                fatal("Unrecognized segment type");
            }
        }
    }

    free(opcodes);
    for(int i = 0; i < num_files; i++) {
        fr_destroy(state[i]);
        bc_prescan_destroy(prescan[i]);
    }

    return result;
}
