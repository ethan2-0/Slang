#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "common.h"
#include "opcodes.h"
#include "interpreter.h"
#include "slbstdlib.h"

uint32_t bc_resolve_name(it_PROGRAM* program, char* callee_name);
uint32_t bc_assign_method_id(it_PROGRAM* program);

void bc_parse_opcode(fr_STATE* state, it_PROGRAM* program, it_METHOD* method, it_OPCODE* opcode) {
    uint8_t opcode_num = fr_getuint8(state);
    opcode->type = opcode_num;
    opcode->linenum = fr_getuint32(state);
    #if DEBUG
    printf("Opcode %02x\n", opcode_num);
    #endif
    if(opcode_num == OPCODE_LOAD) {
        // LOAD
        it_OPCODE_DATA_LOAD* data = mm_malloc(sizeof(it_OPCODE_DATA_LOAD));
        data->target = fr_getuint32(state);
        data->data = fr_getuint64(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_ZERO) {
        // ZERO
        it_OPCODE_DATA_ZERO* data = mm_malloc(sizeof(it_OPCODE_DATA_ZERO));
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_ADD) {
        // ADD
        it_OPCODE_DATA_ADD* data = mm_malloc(sizeof(it_OPCODE_DATA_ADD));
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_TWOCOMP) {
        // TWOCOMP
        it_OPCODE_DATA_TWOCOMP* data = mm_malloc(sizeof(it_OPCODE_DATA_TWOCOMP));
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_MULT) {
        // MULT
        it_OPCODE_DATA_MULT* data = mm_malloc(sizeof(it_OPCODE_DATA_MULT));
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_MODULO) {
        // MODULO
        it_OPCODE_DATA_MODULO* data = mm_malloc(sizeof(it_OPCODE_DATA_MODULO));
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_CALL) {
        // CALL
        it_OPCODE_DATA_CALL* data = mm_malloc(sizeof(it_OPCODE_DATA_CALL));
        char* callee_name = fr_getstr(state);
        data->callee = bc_resolve_name(program, callee_name);
        free(callee_name);
        data->returnval = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_RETURN) {
        // RETURN
        it_OPCODE_DATA_RETURN* data = mm_malloc(sizeof(it_OPCODE_DATA_RETURN));
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_EQUALS) {
        // EQUALS
        it_OPCODE_DATA_EQUALS* data = mm_malloc(sizeof(it_OPCODE_DATA_EQUALS));
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_INVERT) {
        // INVERT
        it_OPCODE_DATA_INVERT* data = mm_malloc(sizeof(it_OPCODE_DATA_INVERT));
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_LTEQ) {
        // LTEQ
        it_OPCODE_DATA_LTEQ* data = mm_malloc(sizeof(it_OPCODE_DATA_LTEQ));
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_GT) {
        // GT
        it_OPCODE_DATA_GT* data = mm_malloc(sizeof(it_OPCODE_DATA_GT));
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_GOTO) {
        // GOTO
        it_OPCODE_DATA_GOTO* data = mm_malloc(sizeof(it_OPCODE_DATA_GOTO));
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_JF) {
        // JF
        it_OPCODE_DATA_JF* data = mm_malloc(sizeof(it_OPCODE_DATA_JF));
        data->predicate = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_PARAM) {
        // PARAM
        it_OPCODE_DATA_PARAM* data = mm_malloc(sizeof(it_OPCODE_DATA_PARAM));
        data->source = fr_getuint32(state);
        data->target = fr_getuint8(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_GTEQ) {
        // GTEQ
        it_OPCODE_DATA_GTEQ* data = mm_malloc(sizeof(it_OPCODE_DATA_GTEQ));
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_XOR) {
        // XOR
        it_OPCODE_DATA_XOR* data = mm_malloc(sizeof(it_OPCODE_DATA_XOR));
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_AND) {
        // AND
        it_OPCODE_DATA_AND* data = mm_malloc(sizeof(it_OPCODE_DATA_AND));
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_OR) {
        // OR
        it_OPCODE_DATA_OR* data = mm_malloc(sizeof(it_OPCODE_DATA_OR));
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_MOV) {
        // MOV
        it_OPCODE_DATA_MOV* data = mm_malloc(sizeof(it_OPCODE_DATA_MOV));
        data->source = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_NOP) {
        // NOP
        // Do nothing
        ;
    } else if(opcode_num == OPCODE_SEP) {
        // SEP
        // This is a NOP that takes an integer parameter.
        fr_getuint64(state);
    } else if(opcode_num == OPCODE_LT) {
        // LT
        it_OPCODE_DATA_LT* data = mm_malloc(sizeof(it_OPCODE_DATA_LT));
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_NEW) {
        it_OPCODE_DATA_NEW* data = mm_malloc(sizeof(it_OPCODE_DATA_NEW));
        data->clazz = (ts_TYPE_CLAZZ*) ts_get_type(fr_getstr(state));
        if(data->clazz->category != ts_CATEGORY_CLAZZ) {
            fatal("Attempt to instantiate something that isn't a class");
        }
        data->dest = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_ACCESS) {
        it_OPCODE_DATA_ACCESS* data = mm_malloc(sizeof(it_OPCODE_DATA_ACCESS));
        data->clazzreg = fr_getuint32(state);
        ts_TYPE_CLAZZ* clazzreg_type = (ts_TYPE_CLAZZ*) method->register_types[data->clazzreg];
        if(clazzreg_type->category != ts_CATEGORY_CLAZZ) {
            fatal("Attempt to access something that isn't a class");
        }
        data->property_index = ts_get_field_index(clazzreg_type, fr_getstr(state));
        data->destination = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_ASSIGN) {
        it_OPCODE_DATA_ASSIGN* data = mm_malloc(sizeof(it_OPCODE_DATA_ASSIGN));
        data->clazzreg = fr_getuint32(state);
        ts_TYPE_CLAZZ* clazzreg_type = (ts_TYPE_CLAZZ*) method->register_types[data->clazzreg];
        if(clazzreg_type->category != ts_CATEGORY_CLAZZ) {
            fatal("Attempt to assign to a property of something that isn't a class");
        }
        data->property_index = ts_get_field_index(clazzreg_type, fr_getstr(state));
        data->source = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_CLASSCALL) {
        it_OPCODE_DATA_CLASSCALL* data = mm_malloc(sizeof(it_OPCODE_DATA_CLASSCALL));
        data->targetreg = fr_getuint32(state);
        ts_TYPE_CLAZZ* clazzreg_type = (ts_TYPE_CLAZZ*) method->register_types[data->targetreg];
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
        it_OPCODE_DATA_ARRALLOC* data = mm_malloc(sizeof(it_OPCODE_DATA_ARRALLOC));
        data->arrreg = fr_getuint32(state);
        data->lengthreg = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_ARRACCESS) {
        it_OPCODE_DATA_ARRACCESS* data = mm_malloc(sizeof(it_OPCODE_DATA_ARRACCESS));
        data->arrreg = fr_getuint32(state);
        data->indexreg = fr_getuint32(state);
        data->elementreg = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_ARRASSIGN) {
        it_OPCODE_DATA_ARRASSIGN* data = mm_malloc(sizeof(it_OPCODE_DATA_ARRASSIGN));
        data->arrreg = fr_getuint32(state);
        data->indexreg = fr_getuint32(state);
        data->elementreg = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_ARRLEN) {
        it_OPCODE_DATA_ARRLEN* data = mm_malloc(sizeof(it_OPCODE_DATA_ARRLEN));
        data->arrreg = fr_getuint32(state);
        data->resultreg = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_DIV) {
        it_OPCODE_DATA_DIV* data = mm_malloc(sizeof(it_OPCODE_DATA_DIV));
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else {
        fatal("Unrecognized opcode");
    }
}
typedef struct {
    int num_methods;
    uint32_t entrypoint_id;
    int num_clazzes;
} bc_PRESCAN_RESULTS;
bc_PRESCAN_RESULTS* bc_prescan(fr_STATE* state) {
    #if DEBUG
    printf("Entering bc_prescan\n");
    #endif
    bc_PRESCAN_RESULTS* results = mm_malloc(sizeof(bc_PRESCAN_RESULTS));
    results->num_methods = 0;
    results->num_clazzes = 0;

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
        }
    }

    fr_rewind(state);
    #if DEBUG
    printf("Exiting bc_prescan\n");
    #endif
    return results;
}
uint32_t bc_assign_method_id(it_PROGRAM* program) {
    return program->method_id++;
}
void bc_prescan_destroy(bc_PRESCAN_RESULTS* prescan) {
    free(prescan);
}
uint32_t bc_resolve_name(it_PROGRAM* program, char* name) {
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
    fatal("Asked to resolve unknown name");
    // Unreachable
    return -1;
}
void bc_parse_method(fr_STATE* state, it_OPCODE* opcode_buff, it_PROGRAM* program, it_METHOD* result) {
    result->replacement_ptr = NULL;
    uint32_t length = fr_getuint32(state);
    uint32_t registerc = fr_getuint32(state);
    uint32_t nargs = fr_getuint32(state);
    char* name = fr_getstr(state);
    uint32_t id = bc_resolve_name(program, name);
    uint32_t bytecode_length = length - 12 - strlen(name);
    size_t end_index = state->index + bytecode_length;
    #if DEBUG
    printf("Length %d\n", length);
    printf("Register count %d\n", registerc);
    printf("Id %d\n", id);
    printf("Nargs %d\n", nargs);
    printf("End index %d\n", end_index);
    #endif
    free(fr_getstr(state));
    result->returntype = ts_get_type(fr_getstr(state));
    result->register_types = mm_malloc(sizeof(ts_TYPE*) * registerc);
    for(int i = 0; i < registerc; i++) {
        char* typename = fr_getstr(state);
        #if DEBUG
        printf("Got register type '%s'\n", typename);
        #endif
        result->register_types[i] = ts_get_type(typename);
    }
    int num_opcodes = 0;
    while(state->index < end_index) {
        bc_parse_opcode(state, program, result, &opcode_buff[num_opcodes]);
        num_opcodes++;
    }
    result->registerc = registerc;
    result->id = id;
    result->nargs = nargs;
    result->opcodec = num_opcodes;
    #if DEBUG
    printf("Opcodec: %d\n", result->opcodec);
    #endif
    result->opcodes = mm_malloc(sizeof(it_OPCODE) * num_opcodes);
    #if DEBUG
    for(int i = 0; i < num_opcodes; i++) {
        printf("Opcode--: %02x\n", opcode_buff[i].type);
    }
    #endif
    memcpy(result->opcodes, opcode_buff, sizeof(it_OPCODE) * num_opcodes);

    #if DEBUG
    printf("Done parsing method\n");
    #endif
}
void bc_scan_types(it_PROGRAM* program, bc_PRESCAN_RESULTS* prescan, fr_STATE* state) {
    while(!fr_iseof(state)) {
        uint8_t segment_type = fr_getuint8(state);
        if(segment_type == SEGMENT_TYPE_METHOD) {
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
            ts_TYPE_CLAZZ* clazz = mm_malloc(sizeof(ts_TYPE_CLAZZ));
            clazz->category = ts_CATEGORY_CLAZZ;
            clazz->id = ts_allocate_type_id();
            clazz->name = strdup(clazzname);
            clazz->heirarchy_len = 1;
            clazz->heirarchy = mm_malloc(sizeof(ts_TYPE*) * 1);
            clazz->heirarchy[0] = (ts_TYPE*) clazz;
            clazz->nfields = -1;
            clazz->fields = NULL;
            ts_register_type((ts_TYPE*) clazz, strdup(clazzname));
            program->clazzes[program->clazz_index++] = clazz;
            free(clazzname);
            free(parentname);
        }
    }
    fr_rewind(state);
    fr_getuint32(state); // magic number
    while(!fr_iseof(state)) {
        uint8_t segment_type = fr_getuint8(state);
        if(segment_type == SEGMENT_TYPE_METHOD) {
            fr_advance(state, fr_getuint32(state));
        } else if(segment_type == SEGMENT_TYPE_METADATA) {
            free(fr_getstr(state));
            free(fr_getstr(state));
        } else if(segment_type == SEGMENT_TYPE_CLASS) {
            fr_getuint32(state);
            char* clazzname = fr_getstr(state);
            char* supertype_name = fr_getstr(state);
            ts_TYPE_CLAZZ* clazz = (ts_TYPE_CLAZZ*) ts_get_type(clazzname);
            if(clazz->category != ts_CATEGORY_CLAZZ) {
                fatal("Tried to extend something that isn't a class");
            }
            if(strlen(supertype_name) > 0) {
                clazz->immediate_supertype = (ts_TYPE_CLAZZ*) ts_get_type(supertype_name);
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
            clazz->fields = mm_malloc(sizeof(ts_CLAZZ_FIELD) * numfields);
            for(uint32_t i = 0; i < numfields; i++) {
                clazz->fields[i].name = fr_getstr(state);
                clazz->fields[i].type = ts_get_type(fr_getstr(state));
            }
            #if DEBUG
            printf("Scanning class '%s'\n", clazzname);
            #endif
            free(supertype_name);
            free(clazzname);
        }
    }
}
void bc_scan_methods(it_PROGRAM* program, fr_STATE* state, int offset) {
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
            it_METHOD* method = &program->methods[methodindex++];
            method->registerc = fr_getuint32(state); // num_registers
            method->id = bc_assign_method_id(program); // id
            method->nargs = fr_getuint32(state); // nargs
            method->name = fr_getstr(state); // name
            char* containing_clazz_name = fr_getstr(state);
            if(strlen(containing_clazz_name) > 0) {
                method->containing_clazz = (ts_TYPE_CLAZZ*) ts_get_type(containing_clazz_name);
            } else {
                method->containing_clazz = NULL;
            }
            free(containing_clazz_name);
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
        } else if(segment_type == SEGMENT_TYPE_CLASS) {
            uint32_t length = fr_getuint32(state);
            fr_advance(state, (int) length);
        } else {
            #if DEBUG
            printf("Segment type %02x\n", segment_type);
            #endif
            fatal("Unknown segment type");
        }
    }
    #if DEBUG
    printf("Exiting bc_scan_methods()\n");
    #endif
}
it_PROGRAM* bc_parse_from_files(int fpc, FILE* fp[]) {
    #if DEBUG
    printf("Parsing %d files\n", fpc);
    #endif
    int num_files = fpc + 1;
    fr_STATE* state[num_files];
    for(int i = 0; i < fpc; i++) {
        state[i] = fr_new(fp[i]);
    }
    state[fpc] = fr_new_from_buffer((size_t) (sl_stdlib_end - sl_stdlib), sl_stdlib);
    // This is the index of the start of the segments in the input bytecode
    bc_PRESCAN_RESULTS* prescan[num_files];
    it_PROGRAM* result = mm_malloc(sizeof(it_PROGRAM));
    result->clazz_index = 0;
    result->method_id = 0;
    result->methodc = NUM_REPLACED_METHODS;
    result->entrypoint = -1;
    for(int i = 0; i < num_files; i++) {
        prescan[i] = bc_prescan(state[i]);
        fr_rewind(state[i]);
        // Ignore the magic number
        fr_getuint32(state[i]);
        result->methodc += prescan[i]->num_methods;
    }
    result->clazzesc = 0;
    for(int i = 0; i < num_files; i++) {
        result->clazzesc += prescan[i]->num_clazzes;
    }
    #if DEBUG
    printf("%d methods\n", result->methodc);
    #endif
    result->methods = mm_malloc(sizeof(it_METHOD) * result->methodc);
    result->clazzes = mm_malloc(sizeof(ts_TYPE_CLAZZ*) * result->clazzesc);
    // typedef struct {
    //     int num_methods;
    //     uint32_t entrypoint_id;
    // } bc_PRESCAN_RESULTS;
    for(int i = 0; i < num_files; i++) {
        bc_scan_types(result, prescan[i], state[i]);
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
    it_replace_methods(result);
    // bc_arrange_type_method_pointers(result);
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
    it_OPCODE* opcodes = mm_malloc(sizeof(it_OPCODE) * largest_bufflen);
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
