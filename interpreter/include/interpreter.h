#include <stdint.h>

#ifndef INTERPRETER_H
#define INTERPRETER_H

union ts_TYPE;
struct it_METHOD;

enum ts_CATEGORY {
    ts_CATEGORY_PRIMITIVE,
    ts_CATEGORY_CLAZZ
};
struct ts_TYPE_BAREBONES {
    enum ts_CATEGORY category;
    uint32_t id;
    int heirarchy_len;
    // This is a pointer to an array of pointers
    union ts_TYPE** heirarchy;
};
struct ts_CLAZZ_FIELD {
    char* name;
    union ts_TYPE* type;
};
struct ts_TYPE_CLAZZ {
    enum ts_CATEGORY category;
    uint32_t id;
    int heirarchy_len;
    // This is a pointer to an array of pointers
    union ts_TYPE** heirarchy;
    int nfields;
    struct ts_CLAZZ_FIELD* fields;
    // This is a pointer to an array of pointers
    struct it_METHOD** methods;
    size_t methodc;
};
union ts_TYPE {
    struct ts_TYPE_BAREBONES barebones;
    struct ts_TYPE_CLAZZ clazz;
};

typedef union ts_TYPE ts_TYPE;
typedef struct ts_TYPE_BAREBONES ts_TYPE_BAREBONES;
typedef struct ts_TYPE_CLAZZ ts_TYPE_CLAZZ;
typedef struct ts_CLAZZ_FIELD ts_CLAZZ_FIELD;
typedef enum ts_CATEGORY ts_CATEGORY;

ts_TYPE* ts_get_type(char* name);
int ts_get_field_index(ts_TYPE_CLAZZ* clazz, char* name);
void ts_register_type(ts_TYPE* type, char* name);
uint32_t ts_allocate_type_id();


union itval {
    uint64_t number;
    // This is a pointer to an array of itval
    union itval* itval;
};

typedef union itval itval;

typedef struct {
    uint8_t type;
    void* payload;
} it_OPCODE;
typedef struct {
    uint32_t id;
    // This could technically be a uint8_t, but ehh
    int nargs;
    int opcodec;
    char* name;
    uint32_t registerc;
    it_OPCODE* opcodes;
    ts_TYPE* returntype;
    // This is a pointer to an array
    ts_TYPE** argument_types;
    ts_TYPE_CLAZZ* containing_clazz;
} it_METHOD;
typedef struct {
    // This is the number of methods
    int methodc;
    // This is the methods, in an arbitrary order
    it_METHOD* methods;
    // This is the current method ID
    uint32_t method_id;
    // This is the entrypoint, as an index into `methods`.
    int entrypoint;
    // This is a pointer to an array of pointers of classes
    ts_TYPE_CLAZZ** clazzes;
    int clazzesc;
} it_PROGRAM;

void it_RUN(it_PROGRAM* prog);

#endif
