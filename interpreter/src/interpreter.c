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
    int registers_allocated;
    uint32_t returnreg;
    it_OPCODE* iptr;
    it_METHOD* method;
} it_STACKFRAME;
void load_method(it_STACKFRAME* stackptr, it_METHOD* method) {
    stackptr->method = method;
    stackptr->iptr = stackptr->method->opcodes;
    stackptr->registerc = stackptr->method->registerc;
    if(stackptr->registers_allocated < stackptr->registerc) {
        #if DEBUG
        printf("Allocating more register space: %d < %d\n", stackptr->registers_allocated, stackptr->registerc);
        #endif
        free(stackptr->registers);
        stackptr->registers = malloc(sizeof(itval) * stackptr->registerc);
    }
    memset(stackptr->registers, 0x00, stackptr->registerc * sizeof(itval));
}
void it_execute(it_PROGRAM* prog) {
    // Setup interpreter state
    it_STACKFRAME* stack = malloc(sizeof(it_STACKFRAME) * STACKSIZE);
    it_STACKFRAME* stackptr = stack;
    it_STACKFRAME* stackend = stack + STACKSIZE;
    for(int i = 0; i < STACKSIZE; i++) {
        stack[i].registers_allocated = 0;
        // Set the register file pointer to the null pointer so we can free() it.
        stack[i].registers = 0;
    }
    load_method(stackptr, &prog->methods[prog->entrypoint]);
    itval* registers = stackptr->registers;
    #if DEBUG
    printf("About to execute method %d\n", stackptr->method->id);
    #endif
    // This is the start of the instruction tape
    it_OPCODE* instruction_start = stackptr->iptr;
    // This is the current index into the instruction tape
    it_OPCODE* iptr = instruction_start;
    // See the documentation for PARAM and CALL for details.
    // Note that this is preserved globally across method calls.
    itval params[256];

    // TODO: Do something smarter for dispatch here
    while(1) {
        #if DEBUG
        for(int i = 0; i < stackptr->registerc; i++) {
            printf("%02x ", registers[i]);
        }
        printf("\n");
        printf("Type %02x, iptr %d\n", iptr->type, iptr - instruction_start);
        #endif
        switch(iptr->type) {
        case OPCODE_PARAM:
            params[((it_OPCODE_DATA_PARAM*) iptr->payload)->target] = registers[((it_OPCODE_DATA_PARAM*) iptr->payload)->source];
            iptr++;
            continue;
        case OPCODE_CALL:
            ; // Labels can't be immediately followed by declarations, because
              // C is a barbaric language
            it_STACKFRAME* oldstack = stackptr;
            stackptr++;
            if(stackptr >= stackend) {
                fatal("Stack overflow");
            }
            uint32_t callee_id = ((it_OPCODE_DATA_CALL*) iptr->payload)->callee;

            #if DEBUG
            printf("Calling %d\n", callee_id);
            #endif

            oldstack->returnreg = ((it_OPCODE_DATA_CALL*) iptr->payload)->returnval;
            it_METHOD* callee = &prog->methods[callee_id];

            #if DEBUG
            printf("Need %d args.\n", callee->nargs);
            for(uint32_t i = 0; i < callee->nargs; i++) {
                printf("Argument: %02x\n", params[i]);
            }
            #endif

            load_method(stackptr, callee);
            registers = stackptr->registers;
            instruction_start = stackptr->iptr;
            oldstack->iptr = iptr;
            iptr = instruction_start;
            for(uint32_t i = 0; i < callee->nargs; i++) {
                registers[i] = params[i];
            }
            continue;
        case OPCODE_RETURN:
            ; // Labels can't be immediately followed by declarations, because
              // C is a barbaric language
            itval result = registers[((it_OPCODE_DATA_RETURN*) iptr->payload)->target];
            if(stackptr <= stack) {
                printf("Returned 0x%08x\n", result);
                goto CLEANUP;
            }
            #if DEBUG
            printf("Returning\n");
            #endif
            stackptr--;
            registers = stackptr->registers;
            instruction_start = stackptr->method->opcodes;
            iptr = stackptr->iptr;
            registers[stackptr->returnreg] = result;
            iptr++;
            continue;
        case OPCODE_ZERO:
            registers[((it_OPCODE_DATA_ZERO*) iptr->payload)->target] = 0x00000000;
            iptr++;
            continue;
        case OPCODE_ADD:
            registers[((it_OPCODE_DATA_ADD*) iptr->payload)->target] = registers[((it_OPCODE_DATA_ADD*) iptr->payload)->source1] + registers[((it_OPCODE_DATA_ADD*) iptr->payload)->source2];
            iptr++;
            continue;
        case OPCODE_TWOCOMP:
            // TODO: There's a compiler bug to do with TWOCOMP and the fact it's an inplace operation.
            //       I should change TWOCOMP to use a source and a destination like with other instructions like MOV.
            registers[((it_OPCODE_DATA_TWOCOMP*) iptr->payload)->target] = -registers[((it_OPCODE_DATA_TWOCOMP*) iptr->payload)->target];
            iptr++;
            continue;
        case OPCODE_MULT:
            registers[((it_OPCODE_DATA_MULT*) iptr->payload)->target] = registers[((it_OPCODE_DATA_MULT*) iptr->payload)->source1] * registers[((it_OPCODE_DATA_MULT*) iptr->payload)->source2];
            iptr++;
            continue;
        case OPCODE_MODULO:
            registers[((it_OPCODE_DATA_MODULO*) iptr->payload)->target] = registers[((it_OPCODE_DATA_MODULO*) iptr->payload)->source1] % registers[((it_OPCODE_DATA_MODULO*) iptr->payload)->source2];
            iptr++;
            continue;
        case OPCODE_EQUALS:
            if(registers[((it_OPCODE_DATA_EQUALS*) iptr->payload)->source1] == registers[((it_OPCODE_DATA_EQUALS*) iptr->payload)->source2]) {
                registers[((it_OPCODE_DATA_EQUALS*) iptr->payload)->target] = 0x1;
            } else {
                registers[((it_OPCODE_DATA_EQUALS*) iptr->payload)->target] = 0x0;
            }
            iptr++;
            continue;
        case OPCODE_INVERT:
            // TODO: Does the compiler bug with TWOCOMP apply here too?
            if(registers[((it_OPCODE_DATA_INVERT*) iptr->payload)->target] == 0x0) {
                registers[((it_OPCODE_DATA_INVERT*) iptr->payload)->target] = 0x1;
            } else {
                registers[((it_OPCODE_DATA_INVERT*) iptr->payload)->target] = 0x0;
            }
            iptr++;
            continue;
        case OPCODE_LTEQ:
            if(registers[((it_OPCODE_DATA_LTEQ*) iptr->payload)->source1] <= registers[((it_OPCODE_DATA_LTEQ*) iptr->payload)->source2]) {
                registers[((it_OPCODE_DATA_LTEQ*) iptr->payload)->target] = 0x1;
            } else {
                registers[((it_OPCODE_DATA_LTEQ*) iptr->payload)->target] = 0x0;
            }
            iptr++;
            continue;
        case OPCODE_GT:
            if(registers[((it_OPCODE_DATA_GT*) iptr->payload)->source1] > registers[((it_OPCODE_DATA_GT*) iptr->payload)->source2]) {
                registers[((it_OPCODE_DATA_GT*) iptr->payload)->target] = 0x1;
            } else {
                registers[((it_OPCODE_DATA_GT*) iptr->payload)->target] = 0x0;
            }
            iptr++;
            continue;
        case OPCODE_GOTO:
            #if DEBUG
            printf("Jumping to %d\n", ((it_OPCODE_DATA_GOTO*) iptr->payload)->target);
            #endif
            iptr = instruction_start + ((it_OPCODE_DATA_GOTO*) iptr->payload)->target;
            continue;
        case OPCODE_JF:
            // JF stands for Jump if False.
            if(registers[((it_OPCODE_DATA_JF*) iptr->payload)->predicate] == 0x00) {
                #if DEBUG
                printf("Jumping to %d\n", ((it_OPCODE_DATA_JF*) iptr->payload)->target);
                #endif
                iptr = instruction_start + ((it_OPCODE_DATA_JF*) iptr->payload)->target;
            } else {
                iptr++;
            }
            continue;
        case OPCODE_GTEQ:
            if(registers[((it_OPCODE_DATA_GTEQ*) iptr->payload)->source1] >= registers[((it_OPCODE_DATA_GTEQ*) iptr->payload)->source2]) {
                registers[((it_OPCODE_DATA_GTEQ*) iptr->payload)->target] = 0x1;
            } else {
                registers[((it_OPCODE_DATA_GTEQ*) iptr->payload)->target] = 0x0;
            }
            iptr++;
            continue;
        case OPCODE_XOR:
            registers[((it_OPCODE_DATA_XOR*) iptr->payload)->target] = registers[((it_OPCODE_DATA_XOR*) iptr->payload)->source1] ^ registers[((it_OPCODE_DATA_XOR*) iptr->payload)->source2];
            iptr++;
            continue;
        case OPCODE_AND:
            registers[((it_OPCODE_DATA_AND*) iptr->payload)->target] = registers[((it_OPCODE_DATA_AND*) iptr->payload)->source1] & registers[((it_OPCODE_DATA_AND*) iptr->payload)->source2];
            iptr++;
            continue;
        case OPCODE_OR:
            registers[((it_OPCODE_DATA_OR*) iptr->payload)->target] = registers[((it_OPCODE_DATA_OR*) iptr->payload)->source1] | registers[((it_OPCODE_DATA_OR*) iptr->payload)->source2];
            iptr++;
            continue;
        case OPCODE_MOV:
            registers[((it_OPCODE_DATA_MOV*) iptr->payload)->target] = registers[((it_OPCODE_DATA_MOV*) iptr->payload)->source];
            iptr++;
            continue;
        case OPCODE_NOP:
            // Do nothing
            iptr++;
            continue;
        case OPCODE_SEP:
            iptr++;
            continue;
        case OPCODE_LOAD:
            registers[((it_OPCODE_DATA_LOAD*) iptr->payload)->target] = ((it_OPCODE_DATA_LOAD*) iptr->payload)->data;
            iptr++;
            continue;
        case OPCODE_LT:
            if(registers[((it_OPCODE_DATA_LT*) iptr->payload)->source1] < registers[((it_OPCODE_DATA_LT*) iptr->payload)->source2]) {
                registers[((it_OPCODE_DATA_LT*) iptr->payload)->target] = 0x1;
            } else {
                registers[((it_OPCODE_DATA_LT*) iptr->payload)->target] = 0x0;
            }
            iptr++;
            continue;
        default:
            fatal("Unexpected opcode");
        }
    }
CLEANUP:
    free(stack);
    // Explicitly not freeing stackptr and stackend since they're dangling pointers at this point
    return;
}
void it_RUN(it_PROGRAM* prog) {
    #if DEBUG
    printf("Entering it_RUN\n");
    #endif
    it_execute(prog);
}
