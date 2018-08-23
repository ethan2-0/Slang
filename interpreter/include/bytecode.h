#include <stdio.h>
#include "interpreter.h"

#ifndef BYTECODE_H
#define BYTECODE_H

it_PROGRAM* bc_parse_from_files(int fpc, FILE* fp[]);

#endif
