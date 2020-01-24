#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "bytecode.h"
#include "opcodes.h"
#include "common.h"
#include "interpreter.h"

int main(int argc, char* argv[]) {
    if(argc < 2) {
        fatal("Not enough arguments\nInclude a bytecode file name\n");
    }
    struct it_OPTIONS options;
    options.print_return_value = false;
    options.gc_verbose = false;
    options.no_gc = false;
    FILE* fp[argc - 1];
    int num_infiles = 0;
    for(int i = 1; i < argc; i++) {
        if(argv[i][0] == '-') {
            if(strcmp(argv[i], "--print-return-value") == 0) {
                options.print_return_value = true;
            } else if(strcmp(argv[i], "--gc-verbose") == 0) {
                options.gc_verbose = true;
            } else if(strcmp(argv[i], "--no-gc") == 0) {
                options.no_gc = true;
            } else {
                printf("Argument: %s\n", argv[i]);
                fatal("Invalid command line argument");
            }
        } else {
            fp[num_infiles] = fopen(argv[i], "r");
            if(fp[num_infiles] == NULL) {
                fatal("Error reading file.");
            }
            num_infiles++;
        }
    }
    struct it_PROGRAM* prog = bc_parse_from_files(num_infiles, fp, &options);
    #if DEBUG
    printf("\n\nMethods: %d\n", prog->methodc);
    printf("Entrypoint: %d\n", prog->entrypoint);
    for(int i = 0; i < prog->methodc; i++) {
        struct it_METHOD* method = &prog->methods[i];
        printf("\nFor method %d\n", method->id);
        printf("Nargs, opcodec %d, %d\n", method->nargs, method->opcodec);
        for(int j = 0; j < method->opcodec; j++) {
            printf("Opcode %02x\n", method->opcodes[j].type);
        }
    }
    #endif
    // TODO: Properly destroy `prog`
    it_run(prog, &options);
}
