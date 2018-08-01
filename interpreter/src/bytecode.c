#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "common.h"
#include "opcodes.h"
#include "interpreter.h"

uint32_t bc_resolve_name(it_PROGRAM* program, char* callee_name);
uint32_t bc_assign_method_id(it_PROGRAM* program);

void bc_parse_opcode(fr_STATE* state, it_PROGRAM* program, it_OPCODE* opcode) {
    uint8_t opcode_num = fr_getuint8(state);
    opcode->type = opcode_num;
    #if DEBUG
    printf("Opcode %02x\n", opcode_num);
    #endif
    if(opcode_num == OPCODE_LOAD) {
        // LOAD
        it_OPCODE_DATA_LOAD* data = malloc(sizeof(it_OPCODE_DATA_LOAD));
        data->target = fr_getuint32(state);
        data->data = fr_getuint64(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_ZERO) {
        // ZERO
        it_OPCODE_DATA_ZERO* data = malloc(sizeof(it_OPCODE_DATA_ZERO));
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_ADD) {
        // ADD
        it_OPCODE_DATA_ADD* data = malloc(sizeof(it_OPCODE_DATA_ADD));
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_TWOCOMP) {
        // TWOCOMP
        it_OPCODE_DATA_TWOCOMP* data = malloc(sizeof(it_OPCODE_DATA_TWOCOMP));
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_MULT) {
        // MULT
        it_OPCODE_DATA_MULT* data = malloc(sizeof(it_OPCODE_DATA_MULT));
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_MODULO) {
        // MODULO
        it_OPCODE_DATA_MODULO* data = malloc(sizeof(it_OPCODE_DATA_MODULO));
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_CALL) {
        // CALL
        it_OPCODE_DATA_CALL* data = malloc(sizeof(it_OPCODE_DATA_CALL));
        char* callee_name = fr_getstr(state);
        data->callee = bc_resolve_name(program, callee_name);
        data->returnval = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_RETURN) {
        // RETURN
        it_OPCODE_DATA_RETURN* data = malloc(sizeof(it_OPCODE_DATA_RETURN));
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_EQUALS) {
        // EQUALS
        it_OPCODE_DATA_EQUALS* data = malloc(sizeof(it_OPCODE_DATA_EQUALS));
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_INVERT) {
        // INVERT
        it_OPCODE_DATA_INVERT* data = malloc(sizeof(it_OPCODE_DATA_INVERT));
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_LTEQ) {
        // LTEQ
        it_OPCODE_DATA_LTEQ* data = malloc(sizeof(it_OPCODE_DATA_LTEQ));
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_GT) {
        // GT
        it_OPCODE_DATA_GT* data = malloc(sizeof(it_OPCODE_DATA_GT));
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_GOTO) {
        // GOTO
        it_OPCODE_DATA_GOTO* data = malloc(sizeof(it_OPCODE_DATA_GOTO));
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_JF) {
        // JF
        it_OPCODE_DATA_JF* data = malloc(sizeof(it_OPCODE_DATA_JF));
        data->predicate = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_PARAM) {
        // PARAM
        it_OPCODE_DATA_PARAM* data = malloc(sizeof(it_OPCODE_DATA_PARAM));
        data->source = fr_getuint32(state);
        data->target = fr_getuint8(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_GTEQ) {
        // GTEQ
        it_OPCODE_DATA_GTEQ* data = malloc(sizeof(it_OPCODE_DATA_GTEQ));
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_XOR) {
        // XOR
        it_OPCODE_DATA_XOR* data = malloc(sizeof(it_OPCODE_DATA_XOR));
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_AND) {
        // AND
        it_OPCODE_DATA_AND* data = malloc(sizeof(it_OPCODE_DATA_AND));
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_OR) {
        // OR
        it_OPCODE_DATA_OR* data = malloc(sizeof(it_OPCODE_DATA_OR));
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    } else if(opcode_num == OPCODE_MOV) {
        // MOV
        it_OPCODE_DATA_MOV* data = malloc(sizeof(it_OPCODE_DATA_MOV));
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
        it_OPCODE_DATA_LT* data = malloc(sizeof(it_OPCODE_DATA_LT));
        data->source1 = fr_getuint32(state);
        data->source2 = fr_getuint32(state);
        data->target = fr_getuint32(state);
        opcode->payload = data;
    }
}
typedef struct {
    int num_methods;
    uint32_t entrypoint_id;
} bc_PRESCAN_RESULTS;
bc_PRESCAN_RESULTS* bc_prescan(fr_STATE* state) {
    bc_PRESCAN_RESULTS* results = malloc(sizeof(bc_PRESCAN_RESULTS));
    results->num_methods = 0;

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
            fr_getstr(state);
            fr_getstr(state);
        }
    }

    fr_rewind(state);
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
    fatal("Asked to resolve unknown name");
    // Unreachable
    return -1;
}
void bc_parse_method(fr_STATE* state, it_OPCODE* opcode_buff, it_PROGRAM* program, it_METHOD* result) {
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
    printf("Name %s\n", result->name);
    printf("End index %d\n", end_index);
    #endif
    int num_opcodes = 0;
    while(state->index < end_index) {
        bc_parse_opcode(state, program, &opcode_buff[num_opcodes]);
        num_opcodes++;
    }
    result->registerc = registerc;
    result->id = id;
    result->nargs = nargs;
    result->opcodec = num_opcodes;
    #if DEBUG
    printf("Sizeof: %d\n", sizeof(it_OPCODE));
    #endif
    result->opcodes = malloc(sizeof(it_OPCODE) * num_opcodes);
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
void bc_scan_methods(it_PROGRAM* program, fr_STATE* state, int offset) {
    // state->methodc and state->methods are already initialized
    #if DEBUG
    printf("Entering bc_scan_methods()\n");
    #endif
    int methodindex = offset;
    while(!fr_iseof(state)) {
        uint8_t segment_type = fr_getuint8(state);
        if(segment_type == SEGMENT_TYPE_METHOD) {
            uint32_t length = fr_getuint32(state);
            uint32_t end_index = state->index + length;
            it_METHOD* method = &program->methods[methodindex++];
            method->registerc = fr_getuint32(state); // num_registers
            method->id = bc_assign_method_id(program); // id
            method->nargs = fr_getuint32(state); // nargs
            method->name = fr_getstr(state); // name
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
            fr_getstr(state);
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
    fr_STATE* state[fpc];
    for(int i = 0; i < fpc; i++) {
        state[i] = fr_new(fp[i]);
    }
    // This is the index of the start of the segments in the input bytecode
    bc_PRESCAN_RESULTS* prescan[fpc];
    it_PROGRAM* result = malloc(sizeof(it_PROGRAM));
    result->method_id = 0;
    result->methodc = 0;
    result->entrypoint = -1;
    for(int i = 0; i < fpc; i++) {
        prescan[i] = bc_prescan(state[i]);
        fr_rewind(state[i]);
        // Ignore the magic number
        fr_getuint32(state[i]);
        result->methodc += prescan[i]->num_methods;
    }
    #if DEBUG
    printf("%d methods\n", result->methodc);
    #endif
    result->methods = malloc(sizeof(it_METHOD) * result->methodc);
    // typedef struct {
    //     int num_methods;
    //     uint32_t entrypoint_id;
    // } bc_PRESCAN_RESULTS;

    for(int i = 0, method_index = 0; i < fpc; i++) {
        bc_scan_methods(result, state[i], method_index);
        method_index += prescan[i]->num_methods;
        fr_rewind(state[i]);
        // Ignore the magic number
        fr_getuint32(state[i]);
    }

    if(result->entrypoint == -1) {
        fatal("No entrypoint found");
    }

    // Reset the index into the input bytecode to the beginning of the segments
    int method_index = 0;
    int largest_bufflen = 0;
    for(int i = 0; i < fpc; i++) {
        if(state[i]->bufflen > largest_bufflen) {
            largest_bufflen = state[i]->bufflen;
        }
    }
    it_OPCODE* opcodes = malloc(sizeof(it_OPCODE) * largest_bufflen);
    for(int i = 0; i < fpc; i++) {
        while(!fr_iseof(state[i])) {
            uint8_t segment_type = fr_getuint8(state[i]);
            if(segment_type == SEGMENT_TYPE_METHOD) {
                bc_parse_method(state[i], opcodes, result, &result->methods[method_index++]);
            } else if(segment_type == SEGMENT_TYPE_METADATA) {
                // Metadata segment
                fr_getstr(state[i]);
                fr_getstr(state[i]);
            }
        }
    }

    free(opcodes);
    for(int i = 0; i < fpc; i++) {
        fr_destroy(state[i]);
        bc_prescan_destroy(prescan[i]);
    }

    return result;
}
