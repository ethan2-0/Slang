#include <stdint.h>
#include <stddef.h>

#ifndef INTERPRETER_H
#define INTERPRETER_H

union ts_TYPE;
struct it_METHOD;

enum ts_CATEGORY {
    ts_CATEGORY_PRIMITIVE,
    ts_CATEGORY_CLAZZ,
    ts_CATEGORY_ARRAY
};
struct ts_TYPE_BAREBONES {
    enum ts_CATEGORY category;
    uint32_t id;
    char* name;
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
    char* name;
    int heirarchy_len;
    // This is a pointer to an array of pointers
    union ts_TYPE** heirarchy;
    struct ts_TYPE_CLAZZ* immediate_supertype;
    int nfields;
    struct ts_CLAZZ_FIELD* fields;
    // This is a pointer to an array of pointers
    struct it_METHOD** methods;
    size_t methodc;
};
struct ts_TYPE_ARRAY {
    enum ts_CATEGORY category;
    uint32_t id;
    char* name;
    int heirarchy_len;
    // This is a pointer to an array of pointers
    union ts_TYPE** heirarchy;
    union ts_TYPE* parent_type;
};
union ts_TYPE {
    struct ts_TYPE_BAREBONES barebones;
    struct ts_TYPE_CLAZZ clazz;
    struct ts_TYPE_ARRAY array;
};

typedef union ts_TYPE ts_TYPE;
typedef struct ts_TYPE_BAREBONES ts_TYPE_BAREBONES;
typedef struct ts_TYPE_CLAZZ ts_TYPE_CLAZZ;
typedef struct ts_CLAZZ_FIELD ts_CLAZZ_FIELD;
typedef struct ts_TYPE_ARRAY ts_TYPE_ARRAY;
typedef enum ts_CATEGORY ts_CATEGORY;

ts_TYPE* ts_get_type(char* name);
int ts_get_field_index(ts_TYPE_CLAZZ* clazz, char* name);
int ts_get_method_index(ts_TYPE_CLAZZ* clazz, char* name);
void ts_register_type(ts_TYPE* type, char* name);
uint32_t ts_allocate_type_id();

struct it_CLAZZ_DATA;
struct it_ARRAY_DATA;

union itval {
    uint64_t number;
    // This is a pointer to an array of itval
    struct it_CLAZZ_DATA* clazz_data;
    struct it_ARRAY_DATA* array_data;
};

struct it_ARRAY_DATA {
    uint64_t length;
    union itval elements[1];
};

struct it_CLAZZ_DATA {
    ts_TYPE_CLAZZ* phi_table;
    union itval itval[1];
};

typedef union itval itval;

typedef struct {
    uint8_t type;
    void* payload;
} it_OPCODE;
struct it_METHOD {
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
};
typedef struct {
    // This is the number of methods
    int methodc;
    // This is the methods, in an arbitrary order
    struct it_METHOD* methods;
    // This is the current method ID
    uint32_t method_id;
    // This is the entrypoint, as an index into `methods`.
    int entrypoint;
    int clazzesc;
    int clazz_index;
    // This is a pointer to an array of pointers of classes
    ts_TYPE_CLAZZ** clazzes;
} it_PROGRAM;

typedef struct it_METHOD it_METHOD;
typedef struct it_ARRAY_DATA it_ARRAY_DATA;

void it_RUN(it_PROGRAM* prog);

void cl_arrange_phi_tables(it_PROGRAM* program);

#endif
