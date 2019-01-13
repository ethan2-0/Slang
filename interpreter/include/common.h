#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#ifndef COMMON_H
#define COMMON_H

#define DEBUG 0

void fatal(char* description);
void fatal_with_errcode(char* description, int errcode);

typedef struct {
    FILE* fp;
    char* buffer;
    size_t bufflen;
    int index;
} fr_STATE;
fr_STATE* fr_new(FILE* fp);
fr_STATE* fr_new_from_buffer(int bufflen, char* buffer);
void fr_destroy(fr_STATE* state);
void fr_advance(fr_STATE* state, int qty);
bool fr_iseof(fr_STATE* state);
char fr_getchar(fr_STATE* state);
void fr_rewind(fr_STATE* state);
uint8_t fr_getuint8(fr_STATE* state);
int8_t fr_getint8(fr_STATE* state);
uint16_t fr_getuint16(fr_STATE* state);
int16_t fr_getint16(fr_STATE* state);
uint32_t fr_getuint32(fr_STATE* state);
int32_t fr_getint32(fr_STATE* state);
uint64_t fr_getuint64(fr_STATE* state);
int64_t fr_getint64(fr_STATE* state);
char* fr_getstr(fr_STATE* state);

void* mm_malloc(size_t size);

#endif
