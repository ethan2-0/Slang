#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "common.h"
#include "opcodes.h"
#include "interpreter.h"

uint32_t bc_resolve_name(it_PROGRAM* program, char* callee_name);

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
    results->entrypoint_id = 0xdeadbeef;

    if(fr_getuint32(state) != 0xcf702b56) {
        fatal("Incorrect bytecode file magic number");
    }

    while(!fr_iseof(state)) {
        uint8_t segment_type = fr_getuint8(state);

        if(segment_type == 0x00) {
            // Length + header + body
            // header = ID + nargs + len(segment.signature.name) + segment.signature.name
            uint32_t length = fr_getuint32(state);
            fr_advance(state, (int) length);
            results->num_methods++;
        } else if(segment_type == 0x01) {
            results->entrypoint_id = fr_getuint32(state);
        }
    }

    fr_rewind(state);
    return results;
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
}
void bc_parse_method(fr_STATE* state, it_OPCODE* opcode_buff, it_PROGRAM* program, it_METHOD* result) {
    uint32_t length = fr_getuint32(state);
    uint32_t registerc = fr_getuint32(state);
    uint32_t id = fr_getuint32(state);
    uint32_t nargs = fr_getuint32(state);
    uint32_t name_length = fr_getuint32(state);
    fr_advance(state, (int) name_length);
    uint32_t bytecode_length = length - 16 - name_length;
    size_t end_index = state->index + bytecode_length;
    #if DEBUG
    printf("Length %d\n", length);
    printf("Register count %d\n", registerc);
    printf("Id %d\n", id);
    printf("Nargs %d\n", nargs);
    printf("Name length %d\n", name_length);
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
void bc_scan_symbols(it_PROGRAM* program, fr_STATE* state) {
    // state->methodc and state->methods are already initialized
    #if DEBUG
    printf("Entering bc_scan_symbols()\n");
    #endif
    int methodindex = 0;
    while(!fr_iseof(state)) {
        uint8_t segment_type = fr_getuint8(state);
        if(segment_type == SEGMENT_TYPE_METHOD) {
            uint32_t length = fr_getuint32(state);
            uint32_t end_index = state->index + length;
            it_METHOD* method = &program->methods[methodindex++];
            method->registerc = fr_getuint32(state); // num_registers
            method->id = fr_getuint32(state); // id
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
            fr_getuint32(state);
        } else {
            #if DEBUG
            printf("Segment type %02x\n", segment_type);
            #endif
            fatal("Unknown segment type");
        }
    }
    #if DEBUG
    printf("Exiting bc_scan_symbols()\n");
    #endif
}
it_PROGRAM* bc_parse_from_file(FILE* fp) {
    fr_STATE* state = fr_new(fp);
    // This is the index of the start of the segments in the input bytecode
    int start_index = state->index;
    bc_PRESCAN_RESULTS* prescan = bc_prescan(state);
    fr_rewind(state);
    // Ignore the magic number
    fr_getuint32(state);
    // typedef struct {
    //     int num_methods;
    //     uint32_t entrypoint_id;
    // } bc_PRESCAN_RESULTS;
    #if DEBUG
    printf("%d methods, entrypoint is %02x\n", prescan->num_methods, prescan->entrypoint_id);
    #endif
    it_PROGRAM* result = malloc(sizeof(it_PROGRAM));
    result->methodc = prescan->num_methods;
    result->methods = malloc(sizeof(it_METHOD) * result->methodc);

    bc_scan_symbols(result, state);
    fr_rewind(state);
    // Ignore the magic number
    fr_getuint32(state);

    it_OPCODE* opcodes = malloc(sizeof(it_OPCODE) * state->bufflen);
    // Reset the index into the input bytecode to the beginning of the segments
    int method_index = 0;
    while(!fr_iseof(state)) {
        uint8_t segment_type = fr_getuint8(state);
        if(segment_type == 0x00) {
            bc_parse_method(state, opcodes, result, &result->methods[method_index++]);
        } else if(segment_type == 0x01) {
            // Metadata segment
            fr_getuint32(state);
        }
    }
    // Assign result->entrypoint
    for(int i = 0; i < result->methodc; i++) {
        if(result->methods[i].id == prescan->entrypoint_id) {
            result->entrypoint = i;
            break;
        }
    }

    free(opcodes);
    fr_destroy(state);
    bc_prescan_destroy(prescan);

    return result;
}
