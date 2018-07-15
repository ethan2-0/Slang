#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "common.h"
#include "interpreter.h"
#include "opcodes.h"

#define STACKSIZE 1024

typedef uint64_t itval;

typedef struct {
    itval* registers;
    int registerc;
    it_OPCODE* iptr;
    it_METHOD* method;
} it_STACKFRAME;
void load_method(it_STACKFRAME* stackptr, it_METHOD* method) {
    stackptr->method = method;
    stackptr->iptr = stackptr->method->opcodes;
    stackptr->registerc = stackptr->method->registerc;
    stackptr->registers = malloc(sizeof(itval) * stackptr->registerc);
}
void it_execute(it_PROGRAM* prog) {
    // Setup interpreter state
    it_STACKFRAME* stack = malloc(sizeof(it_STACKFRAME) * STACKSIZE);
    it_STACKFRAME* stackptr = stack;
    it_STACKFRAME* stackend = stack + sizeof(it_STACKFRAME) * STACKSIZE;
    load_method(stackptr, &prog->methods[prog->entrypoint]);
    itval* registers = stackptr->registers;
    memset(registers, 0x00, stackptr->registerc * sizeof(it_STACKFRAME));
    #if DEBUG
    printf("About to execute method %d\n", stackptr->method->id);
    #endif
    // This is the start of the instruction tape
    it_OPCODE* instruction_start = stackptr->iptr;
    // This is the current index into the instruction tape
    it_OPCODE* iptr = instruction_start;
    // TODO: Do something smarter for dispatch here
    while(1) {
        // sleep(1);
        #if DEBUG
        for(int i = 0; i < stackptr->registerc; i++) {
            printf("%02x ", registers[i]);
        }
        printf("\n");
        printf("Type %02x, iptr %d\n", iptr->type, iptr - instruction_start);
        #endif
        if(iptr->type == OPCODE_ZERO) {
            registers[((it_OPCODE_DATA_ZERO*) iptr->payload)->target] = 0x00000000;
            iptr++;
            continue;
        } else if(iptr->type == OPCODE_ADD) {
            registers[((it_OPCODE_DATA_ADD*) iptr->payload)->target] = registers[((it_OPCODE_DATA_ADD*) iptr->payload)->source1] + registers[((it_OPCODE_DATA_ADD*) iptr->payload)->source2];
            iptr++;
            continue;
        } else if(iptr->type == OPCODE_TWOCOMP) {
            // TODO: There's a compiler bug to do with TWOCOMP and the fact it's an inplace operation.
            //       I should change TWOCOMP to use a source and a destination like with other instructions like MOV.
            registers[((it_OPCODE_DATA_TWOCOMP*) iptr->payload)->target] = -registers[((it_OPCODE_DATA_TWOCOMP*) iptr->payload)->target];
            iptr++;
            continue;
        } else if(iptr->type == OPCODE_MULT) {
            registers[((it_OPCODE_DATA_MULT*) iptr->payload)->target] = registers[((it_OPCODE_DATA_MULT*) iptr->payload)->source1] * registers[((it_OPCODE_DATA_MULT*) iptr->payload)->source2];
            iptr++;
            continue;
        } else if(iptr->type == OPCODE_MODULO) {
            registers[((it_OPCODE_DATA_MODULO*) iptr->payload)->target] = registers[((it_OPCODE_DATA_MODULO*) iptr->payload)->source1] % registers[((it_OPCODE_DATA_MODULO*) iptr->payload)->source2];
            iptr++;
            continue;
        } else if(iptr->type == OPCODE_RETURN) {
            // Eventually we'll do something better here, for now just print the returned value
            itval result = registers[((it_OPCODE_DATA_RETURN*) iptr->payload)->target];
            printf("Returned %08x\n", result);
            break;
        } else if(iptr->type == OPCODE_EQUALS) {
            if(registers[((it_OPCODE_DATA_EQUALS*) iptr->payload)->source1] == registers[((it_OPCODE_DATA_EQUALS*) iptr->payload)->source2]) {
                registers[((it_OPCODE_DATA_EQUALS*) iptr->payload)->target] = 0x1;
            } else {
                registers[((it_OPCODE_DATA_EQUALS*) iptr->payload)->target] = 0x0;
            }
            iptr++;
            continue;
        } else if(iptr->type == OPCODE_INVERT) {
            // TODO: Does the compiler bug with TWOCOMP apply here too?
            if(registers[((it_OPCODE_DATA_INVERT*) iptr->payload)->target] == 0x0) {
                registers[((it_OPCODE_DATA_INVERT*) iptr->payload)->target] = 0x1;
            } else {
                registers[((it_OPCODE_DATA_INVERT*) iptr->payload)->target] = 0x0;
            }
            iptr++;
            continue;
        } else if(iptr->type == OPCODE_LTEQ) {
            if(registers[((it_OPCODE_DATA_LTEQ*) iptr->payload)->source1] <= registers[((it_OPCODE_DATA_LTEQ*) iptr->payload)->source2]) {
                registers[((it_OPCODE_DATA_LTEQ*) iptr->payload)->target] = 0x1;
            } else {
                registers[((it_OPCODE_DATA_LTEQ*) iptr->payload)->target] = 0x0;
            }
            iptr++;
            continue;
        } else if(iptr->type == OPCODE_GT) {
            if(registers[((it_OPCODE_DATA_GT*) iptr->payload)->source1] > registers[((it_OPCODE_DATA_GT*) iptr->payload)->source2]) {
                registers[((it_OPCODE_DATA_GT*) iptr->payload)->target] = 0x1;
            } else {
                registers[((it_OPCODE_DATA_GT*) iptr->payload)->target] = 0x0;
            }
            iptr++;
            continue;
        } else if(iptr->type == OPCODE_GOTO) {
            iptr = instruction_start + ((it_OPCODE_DATA_GOTO*) iptr->payload)->target;
            printf("Jumping to %d\n", ((it_OPCODE_DATA_GOTO*) iptr->payload)->target);
            // if( > registers[((it_OPCODE_DATA_GT*) iptr->payload)->source2]) {
            //     registers[((it_OPCODE_DATA_GT*) iptr->payload)->target] = 0x1;
            // } else {
            //     registers[((it_OPCODE_DATA_GT*) iptr->payload)->target] = 0x0;
            // }
            continue;
        } else if(iptr->type == OPCODE_JF) {
            // JF stands for Jump if False.
            if(registers[((it_OPCODE_DATA_JF*) iptr->payload)->predicate] == 0x00) {
                printf("Jumping to %d\n", ((it_OPCODE_DATA_JF*) iptr->payload)->target);
                iptr = instruction_start + ((it_OPCODE_DATA_JF*) iptr->payload)->target;
            } else {
                iptr++;
            }
            continue;
        } else if(iptr->type == OPCODE_GTEQ) {
            if(registers[((it_OPCODE_DATA_GTEQ*) iptr->payload)->source1] >= registers[((it_OPCODE_DATA_GTEQ*) iptr->payload)->source2]) {
                registers[((it_OPCODE_DATA_GTEQ*) iptr->payload)->target] = 0x1;
            } else {
                registers[((it_OPCODE_DATA_GTEQ*) iptr->payload)->target] = 0x0;
            }
            iptr++;
            continue;
        } else if(iptr->type == OPCODE_XOR) {
            registers[((it_OPCODE_DATA_XOR*) iptr->payload)->target] = registers[((it_OPCODE_DATA_XOR*) iptr->payload)->source1] ^ registers[((it_OPCODE_DATA_XOR*) iptr->payload)->source2];
            iptr++;
            continue;
        } else if(iptr->type == OPCODE_AND) {
            registers[((it_OPCODE_DATA_AND*) iptr->payload)->target] = registers[((it_OPCODE_DATA_AND*) iptr->payload)->source1] & registers[((it_OPCODE_DATA_AND*) iptr->payload)->source2];
            iptr++;
            continue;
        } else if(iptr->type == OPCODE_OR) {
            registers[((it_OPCODE_DATA_OR*) iptr->payload)->target] = registers[((it_OPCODE_DATA_OR*) iptr->payload)->source1] | registers[((it_OPCODE_DATA_OR*) iptr->payload)->source2];
            iptr++;
            continue;
        } else if(iptr->type == OPCODE_MOV) {
            registers[((it_OPCODE_DATA_MOV*) iptr->payload)->target] = registers[((it_OPCODE_DATA_MOV*) iptr->payload)->source];
            iptr++;
            continue;
        } else if(iptr->type == OPCODE_NOP || iptr->type == OPCODE_SEP) {
            // Do nothing
            iptr++;
            continue;
        } else if(iptr->type == OPCODE_LOAD) {
            registers[((it_OPCODE_DATA_LOAD*) iptr->payload)->target] = ((it_OPCODE_DATA_LOAD*) iptr->payload)->data;
            iptr++;
            continue;
        } else if(iptr->type == OPCODE_LT) {
            if(registers[((it_OPCODE_DATA_GTEQ*) iptr->payload)->source1] < registers[((it_OPCODE_DATA_GTEQ*) iptr->payload)->source2]) {
                registers[((it_OPCODE_DATA_GTEQ*) iptr->payload)->target] = 0x1;
            } else {
                registers[((it_OPCODE_DATA_GTEQ*) iptr->payload)->target] = 0x0;
            }
            iptr++;
            continue;
        } else {
            fatal("Unexpected opcode");
        }
    }
    free(stack);
    // Explicitly discarding stackptr and stackend since they're dangling pointers at this point
    return;
}
void it_RUN(it_PROGRAM* prog) {
    #if DEBUG
    printf("Entering it_RUN\n");
    #endif
    it_execute(prog);
}
