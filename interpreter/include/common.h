#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#ifndef COMMON_H
#define COMMON_H

#define DEBUG 0

void fatal(char* description);
void fatal_with_errcode(char* description, int errcode);

struct fr_STATE {
    FILE* fp;
    char* buffer;
    size_t bufflen;
    int index;
};
struct fr_STATE* fr_new(FILE* fp);
struct fr_STATE* fr_new_from_buffer(int bufflen, char* buffer);
void fr_destroy(struct fr_STATE* state);
void fr_advance(struct fr_STATE* state, int qty);
bool fr_iseof(struct fr_STATE* state);
char fr_getchar(struct fr_STATE* state);
void fr_rewind(struct fr_STATE* state);
uint8_t fr_getuint8(struct fr_STATE* state);
int8_t fr_getint8(struct fr_STATE* state);
uint16_t fr_getuint16(struct fr_STATE* state);
int16_t fr_getint16(struct fr_STATE* state);
uint32_t fr_getuint32(struct fr_STATE* state);
int32_t fr_getint32(struct fr_STATE* state);
uint64_t fr_getuint64(struct fr_STATE* state);
int64_t fr_getint64(struct fr_STATE* state);
char* fr_getstr(struct fr_STATE* state);

void* mm_malloc(size_t size);

#endif /* COMMON_H */
