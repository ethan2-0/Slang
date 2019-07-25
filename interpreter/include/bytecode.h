#include <stdio.h>
#include "interpreter.h"

#ifndef BYTECODE_H
#define BYTECODE_H

struct it_PROGRAM* bc_parse_from_files(int fpc, FILE* fp[], struct it_OPTIONS* options);

#endif /* BYTECODE_H */
