#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "bytecode.h"
#include "opcodes.h"
#include "common.h"

int main(int argc, char* argv[]) {
    if(argc < 2) {
        fatal("Not enough arguments\nInclude a bytecode file name\n");
    }
    FILE* fp[argc - 1];
    // fp = fopen(argv[1], "r");
    // if(fp == NULL) {
    //     fatal("Error reading file.");
    // }
    for(int i = 1; i < argc; i++) {
        fp[i - 1] = fopen(argv[i], "r");
        if(fp[i - 1] == NULL) {
            fatal("Error reading file.");
        }
    }
    it_PROGRAM* prog = bc_parse_from_files(argc - 1, fp);
    #if DEBUG
    printf("\n\nMethods: %d\n", prog->methodc);
    printf("Entrypoint: %d\n", prog->entrypoint);
    for(int i = 0; i < prog->methodc; i++) {
        it_METHOD* method = &prog->methods[i];
        printf("\nFor method %d\n", method->id);
        printf("Nargs, opcodec %d, %d\n", method->nargs, method->opcodec);
        for(int j = 0; j < method->opcodec; j++) {
            printf("Opcode %02x\n", method->opcodes[j].type);
        }
    }
    #endif
    // TODO: Properly destroy `prog`
    it_RUN(prog);
}
