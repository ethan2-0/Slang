#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "common.h"

void fatal_with_errcode(char* description, int errcode) {
    printf("Fatal error\n%s", description);
    exit(errcode);
}
void fatal(char* description) {
    fatal_with_errcode(description, EXIT_FAILURE);
}
void assert(bool b) {
    #if ASSERTIONS
    if(!b) {
        fatal("Assertion failed");
    }
    #endif
}

struct fr_STATE* fr_new(FILE* fp) {
    struct fr_STATE* state = malloc(sizeof(struct fr_STATE));
    state->index = 0;
    state->fp = fp;
    fseek(fp, 0, SEEK_END);
    state->bufflen = ftell(fp);
    rewind(fp);
    state->buffer = malloc(state->bufflen);
    fread(state->buffer, 1, state->bufflen, fp);
    fclose(fp);
    return state;
}
struct fr_STATE* fr_new_from_buffer(int bufflen, char* buffer) {
    struct fr_STATE* state = malloc(sizeof(struct fr_STATE));
    state->index = 0;
    state->fp = NULL;
    state->bufflen = bufflen;
    state->buffer = malloc(bufflen * sizeof(char));
    memcpy(state->buffer, buffer, bufflen);
    return state;
}
void fr_destroy(struct fr_STATE* state) {
    free(state->buffer);
    free(state);
}
void fr_advance(struct fr_STATE* state, int qty) {
    state->index += qty;
}
void fr_rewind(struct fr_STATE* state) {
    state->index = 0;
}
bool fr_iseof(struct fr_STATE* state) {
    return state->index >= state->bufflen;
}
char fr_getchar(struct fr_STATE* state) {
    if(fr_iseof(state)) {
        fatal("Read past end of struct fr_STATE->buffer");
        return '\0';
    }
    return state->buffer[state->index++] & 0xff;
}
uint8_t fr_getuint8(struct fr_STATE* state) {
    return fr_getchar(state) & 0xff;
}
int8_t fr_getint8(struct fr_STATE* state) {
    return fr_getchar(state) & 0xff;
}
uint16_t fr_getuint16(struct fr_STATE* state) {
    uint16_t top = fr_getchar(state);
    uint16_t bottom = fr_getchar(state);
    bottom &= 0xff;
    top &= 0xff;
    top <<= 8;
    return bottom + top;
}
int16_t fr_getint16(struct fr_STATE* state) {
    return (int16_t) fr_getuint16(state);
}
uint32_t fr_getuint32(struct fr_STATE* state) {
    uint32_t top = fr_getuint16(state);
    uint32_t bottom = fr_getuint16(state);
    top <<= 16;
    return bottom + top;
}
int32_t fr_getint32(struct fr_STATE* state) {
    return (int32_t) fr_getuint32(state);
}
uint64_t fr_getuint64(struct fr_STATE* state) {
    uint64_t top = fr_getuint32(state);
    uint64_t bottom = fr_getuint32(state);
    top <<= 32;
    return bottom + top;
}
int64_t fr_getint64(struct fr_STATE* state) {
    return (int64_t) fr_getuint64(state);
}
char* fr_getstr(struct fr_STATE* state) {
    uint32_t length = fr_getuint32(state);
    char* buff = malloc(length + 1);
    for(uint32_t i = 0; i < length; i++) {
        buff[i] = fr_getchar(state);
    }
    // Null-terminate the string
    buff[length] = '\0';
    return buff;
}

void* mm_malloc(size_t size) {
    void* buff = malloc(size);
    if(buff == NULL) {
        printf("Allocation size: %ld\n", size);
        fatal("Memory allocation failed");
    }
    memset(buff, 0, size);
    return buff;
}
