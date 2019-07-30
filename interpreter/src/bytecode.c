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
        // LOAD
        struct it_OPCODE_DATA_LOAD* data = mm_malloc(sizeof(struct it_OPCODE_DATA_LOAD));
        data->target = fr_getuint32(state);
        data->data = fr_getuint64(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_ZERO) {
        // ZERO
        struct it_OPCODE_DATA_ZERO* data = mm_malloc(sizeof(struct it_OPCODE_DATA_ZERO));
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_ADD) {
        // ADD
        struct it_OPCODE_DATA_ADD* data = mm_malloc(sizeof(struct it_OPCODE_DATA_ADD));
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_TWOCOMP) {
        // TWOCOMP
        struct it_OPCODE_DATA_TWOCOMP* data = mm_malloc(sizeof(struct it_OPCODE_DATA_TWOCOMP));
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_MULT) {
        // MULT
        struct it_OPCODE_DATA_MULT* data = mm_malloc(sizeof(struct it_OPCODE_DATA_MULT));
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_MODULO) {
        // MODULO
        struct it_OPCODE_DATA_MODULO* data = mm_malloc(sizeof(struct it_OPCODE_DATA_MODULO));
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_CALL) {
        // CALL
        struct it_OPCODE_DATA_CALL* data = mm_malloc(sizeof(struct it_OPCODE_DATA_CALL));
        char* callee_name = fr_getstr(state);
        data->callee = bc_resolve_name(program, callee_name);
        free(callee_name);
        data->returnval = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_RETURN) {
        // RETURN
        struct it_OPCODE_DATA_RETURN* data = mm_malloc(sizeof(struct it_OPCODE_DATA_RETURN));
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_EQUALS) {
        // EQUALS
        struct it_OPCODE_DATA_EQUALS* data = mm_malloc(sizeof(struct it_OPCODE_DATA_EQUALS));
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_INVERT) {
        // INVERT
        struct it_OPCODE_DATA_INVERT* data = mm_malloc(sizeof(struct it_OPCODE_DATA_INVERT));
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_LTEQ) {
        // LTEQ
        struct it_OPCODE_DATA_LTEQ* data = mm_malloc(sizeof(struct it_OPCODE_DATA_LTEQ));
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_GT) {
        // GT
        struct it_OPCODE_DATA_GT* data = mm_malloc(sizeof(struct it_OPCODE_DATA_GT));
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_GOTO) {
        // GOTO
        struct it_OPCODE_DATA_GOTO* data = mm_malloc(sizeof(struct it_OPCODE_DATA_GOTO));
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_JF) {
        // JF
        struct it_OPCODE_DATA_JF* data = mm_malloc(sizeof(struct it_OPCODE_DATA_JF));
        data->predicate = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_PARAM) {
        // PARAM
        struct it_OPCODE_DATA_PARAM* data = mm_malloc(sizeof(struct it_OPCODE_DATA_PARAM));
        data->source = fr_getuint32(state);
        data->target = fr_getuint8(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_GTEQ) {
        // GTEQ
        struct it_OPCODE_DATA_GTEQ* data = mm_malloc(sizeof(struct it_OPCODE_DATA_GTEQ));
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_XOR) {
        // XOR
        struct it_OPCODE_DATA_XOR* data = mm_malloc(sizeof(struct it_OPCODE_DATA_XOR));
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_AND) {
        // AND
        struct it_OPCODE_DATA_AND* data = mm_malloc(sizeof(struct it_OPCODE_DATA_AND));
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_OR) {
        // OR
        struct it_OPCODE_DATA_OR* data = mm_malloc(sizeof(struct it_OPCODE_DATA_OR));
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_MOV) {
        // MOV
        struct it_OPCODE_DATA_MOV* data = mm_malloc(sizeof(struct it_OPCODE_DATA_MOV));
        data->source = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_NOP) {
        // NOP
        // Do nothing
        ;
    } else if(opcode_num == OPCODE_LT) {
        // LT
        struct it_OPCODE_DATA_LT* data = mm_malloc(sizeof(struct it_OPCODE_DATA_LT));
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_NEW) {
        struct it_OPCODE_DATA_NEW* data = mm_malloc(sizeof(struct it_OPCODE_DATA_NEW));
        data->clazz = (struct ts_TYPE_CLAZZ*) ts_get_type(fr_getstr(state));
        if(data->clazz->category != ts_CATEGORY_CLAZZ) {
            fatal("Attempt to instantiate something that isn't a class");
        }
        data->dest = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_ACCESS) {
        struct it_OPCODE_DATA_ACCESS* data = mm_malloc(sizeof(struct it_OPCODE_DATA_ACCESS));
        data->clazzreg = fr_getuint32(state);
        struct ts_TYPE_CLAZZ* clazzreg_type = (struct ts_TYPE_CLAZZ*) method->register_types[data->clazzreg];
        if(clazzreg_type->category != ts_CATEGORY_CLAZZ) {
            fatal("Attempt to access something that isn't a class");
        }
        data->property_index = ts_get_field_index(clazzreg_type, fr_getstr(state));
        data->destination = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_ASSIGN) {
        struct it_OPCODE_DATA_ASSIGN* data = mm_malloc(sizeof(struct it_OPCODE_DATA_ASSIGN));
        data->clazzreg = fr_getuint32(state);
        struct ts_TYPE_CLAZZ* clazzreg_type = (struct ts_TYPE_CLAZZ*) method->register_types[data->clazzreg];
        if(clazzreg_type->category != ts_CATEGORY_CLAZZ) {
            fatal("Attempt to assign to a property of something that isn't a class");
        }
        data->property_index = ts_get_field_index(clazzreg_type, fr_getstr(state));
        data->source = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_CLASSCALL) {
        struct it_OPCODE_DATA_CLASSCALL* data = mm_malloc(sizeof(struct it_OPCODE_DATA_CLASSCALL));
        data->targetreg = fr_getuint32(state);
        struct ts_TYPE_CLAZZ* clazzreg_type = (struct ts_TYPE_CLAZZ*) method->register_types[data->targetreg];
        if(clazzreg_type->category != ts_CATEGORY_CLAZZ) {
            fatal("Attempt to call a method of something that isn't a class");
        }
        char* callee_name = fr_getstr(state);
        // data->callee = bc_resolve_name(program, callee_name);
        data->callee_index = ts_get_method_index(clazzreg_type, callee_name);
        free(callee_name);
        data->returnreg = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_ARRALLOC) {
        struct it_OPCODE_DATA_ARRALLOC* data = mm_malloc(sizeof(struct it_OPCODE_DATA_ARRALLOC));
        data->arrreg = fr_getuint32(state);
        data->lengthreg = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_ARRACCESS) {
        struct it_OPCODE_DATA_ARRACCESS* data = mm_malloc(sizeof(struct it_OPCODE_DATA_ARRACCESS));
        data->arrreg = fr_getuint32(state);
        data->indexreg = fr_getuint32(state);
        data->elementreg = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_ARRASSIGN) {
        struct it_OPCODE_DATA_ARRASSIGN* data = mm_malloc(sizeof(struct it_OPCODE_DATA_ARRASSIGN));
        data->arrreg = fr_getuint32(state);
        data->indexreg = fr_getuint32(state);
        data->elementreg = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_ARRLEN) {
        struct it_OPCODE_DATA_ARRLEN* data = mm_malloc(sizeof(struct it_OPCODE_DATA_ARRLEN));
        data->arrreg = fr_getuint32(state);
        data->resultreg = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_DIV) {
        struct it_OPCODE_DATA_DIV* data = mm_malloc(sizeof(struct it_OPCODE_DATA_DIV));
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_CAST) {
        struct it_OPCODE_DATA_CAST* data = mm_malloc(sizeof(struct it_OPCODE_DATA_CAST));
        data->source = fr_getuint32(state);
        data->target = fr_getuint32(state);
        data->target_type = method->register_types[data->target];
        data->source_register_type = method->register_types[data->target];
        if(data->target_type->barebones.category != ts_CATEGORY_CLAZZ) {
            fatal("Can't cast something that isn't a class. This is a compiler bug.");
        }
        if(data->target_type->barebones.category != ts_CATEGORY_CLAZZ) {
            fatal("Cannot cast to a type that isn't a class. This is a compiler bug.");
        }
        opcode->payload = data;
    } else if(opcode_num == OPCODE_INSTANCEOF) {
        struct it_OPCODE_DATA_INSTANCEOF* data = mm_malloc(sizeof(struct it_OPCODE_DATA_INSTANCEOF));
        data->source = fr_getuint32(state);
        data->destination = fr_getuint32(state);
        char* typename = fr_getstr(state);
        data->source_register_type = method->register_types[data->source];
        data->predicate_type = ts_get_type(typename);
        if(data->predicate_type->barebones.category != ts_CATEGORY_CLAZZ) {
            fatal("Predicate type for instanceof must be a class. This is a compiler bug.");
        }
        free(typename);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_STATICVARGET) {
        struct it_OPCODE_DATA_STATICVARGET* data = mm_malloc(sizeof(struct it_OPCODE_DATA_STATICVARGET));
        char* source_var_name = fr_getstr(state);
        data->source_var = sv_get_static_var_index_by_name(program, source_var_name);
        data->destination = fr_getuint32(state);
        free(source_var_name);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_STATICVARSET) {
        struct it_OPCODE_DATA_STATICVARSET* data = mm_malloc(sizeof(struct it_OPCODE_DATA_STATICVARSET));
        data->source = fr_getuint32(state);
        char* dest_var_name = fr_getstr(state);
        data->destination_var = sv_get_static_var_index_by_name(program, dest_var_name);
        free(dest_var_name);
        opcode->payload = data;
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
union itval bc_create_staticvar_value(struct it_PROGRAM* program, struct fr_STATE* state, union ts_TYPE* type) {
    if(type->barebones.category == ts_CATEGORY_PRIMITIVE) {
        union itval ret;
        ret.number = fr_getuint64(state);
        return ret;
    } else if(type->barebones.category == ts_CATEGORY_ARRAY) {
        uint64_t length = fr_getuint64(state);
        size_t allocation_size = sizeof(struct it_ARRAY_DATA) + (length == 0 ? 0 : (length - 1) * sizeof(union itval));
        struct it_ARRAY_DATA* array_data = mm_malloc(allocation_size);
        array_data->length = length;
        array_data->type = (struct ts_TYPE_ARRAY*) type;
        for(uint64_t i = 0; i < length; i++) {
            array_data->elements[i] = bc_create_staticvar_value(program, state, type->array.parent_type);
        }
        union itval ret;
        ret.array_data = array_data;
        ret.array_data->gc_registry_entry = gc_register_object(ret, allocation_size, ts_CATEGORY_ARRAY);
        return ret;
    } else if(type->barebones.category == ts_CATEGORY_CLAZZ) {
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
                union ts_TYPE* type = ts_get_type(typename);
                union itval value = bc_create_staticvar_value(program, state, type);
                sv_add_static_var(program, type, strdup(name), value);
                free(name);
                free(typename);
            }
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
    free(fr_getstr(state));
    result->returntype = ts_get_type(fr_getstr(state));
    result->register_types = mm_malloc(sizeof(union ts_TYPE*) * registerc);
    for(int i = 0; i < registerc; i++) {
        char* typename = fr_getstr(state);
        #if DEBUG
        printf("Got register type '%s'\n", typename);
        #endif
        result->register_types[i] = ts_get_type(typename);
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
            #if DEBUG
            printf("Prescanning class '%s'\n", clazzname);
            #endif
            uint32_t numfields = fr_getuint32(state);
            for(uint32_t i = 0; i < numfields; i++) {
                free(fr_getstr(state));
                free(fr_getstr(state));
            }
            struct ts_TYPE_CLAZZ* clazz = mm_malloc(sizeof(struct ts_TYPE_CLAZZ));
            clazz->category = ts_CATEGORY_CLAZZ;
            clazz->id = ts_allocate_type_id();
            clazz->name = strdup(clazzname);
            clazz->heirarchy_len = 1;
            clazz->heirarchy = mm_malloc(sizeof(union ts_TYPE*) * 1);
            clazz->heirarchy[0] = (union ts_TYPE*) clazz;
            clazz->nfields = -1;
            clazz->fields = NULL;
            // Check for duplicate class names
            for(int i = 0; i < program->clazz_index; i++) {
                if(strcmp(program->clazzes[i]->name, clazz->name) == 0) {
                    printf("Duplicate class name: '%s'\n", clazz->name);
                    fatal("Duplicate class name");
                }
            }
            ts_register_type((union ts_TYPE*) clazz, strdup(clazzname));
            program->clazzes[program->clazz_index++] = clazz;
            free(clazzname);
            free(parentname);
        } else {
            fatal("Unrecognized segment type");
        }
    }
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
            struct ts_TYPE_CLAZZ* clazz = (struct ts_TYPE_CLAZZ*) ts_get_type(clazzname);
            if(clazz->category != ts_CATEGORY_CLAZZ) {
                fatal("Tried to extend something that isn't a class");
            }
            if(strlen(supertype_name) > 0) {
                clazz->immediate_supertype = (struct ts_TYPE_CLAZZ*) ts_get_type(supertype_name);
                if(clazz->immediate_supertype == NULL) {
                    fatal("Couldn't find superclass");
                }
                if(clazz->immediate_supertype->category != ts_CATEGORY_CLAZZ) {
                    fatal("Tried to extend something that isn't a class");
                }
            } else {
                clazz->immediate_supertype = NULL;
            }
            uint32_t numfields = fr_getuint32(state);
            clazz->nfields = numfields;
            clazz->fields = mm_malloc(sizeof(struct ts_CLAZZ_FIELD) * numfields);
            for(uint32_t i = 0; i < numfields; i++) {
                clazz->fields[i].name = fr_getstr(state);
                clazz->fields[i].type = ts_get_type(fr_getstr(state));
            }
            #if DEBUG
            printf("Scanning class '%s'\n", clazzname);
            #endif
            free(supertype_name);
            free(clazzname);
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
            if(strlen(containing_clazz_name) > 0) {
                method->containing_clazz = (struct ts_TYPE_CLAZZ*) ts_get_type(containing_clazz_name);
            } else {
                method->containing_clazz = NULL;
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
    result->clazzes = mm_malloc(sizeof(struct ts_TYPE_CLAZZ*) * result->clazzesc);
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
    cl_arrange_phi_tables(result);

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
