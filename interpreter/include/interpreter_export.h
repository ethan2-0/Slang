#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef INTERPRETER_EXPORTS_H
#define INTERPRETER_EXPORTS_H

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

enum ts_CATEGORY ts_CATEGORY;


struct it_CLAZZ_DATA;
struct it_ARRAY_DATA;

union itval {
    int64_t number;
    struct it_CLAZZ_DATA* clazz_data;
    struct it_ARRAY_DATA* array_data;
};

struct gc_OBJECT_REGISTRY {
    struct gc_OBJECT_REGISTRY* next;
    struct gc_OBJECT_REGISTRY* previous;
    uint64_t object_id;
    bool is_present;
    size_t allocation_size;
    union itval object;
    enum ts_CATEGORY category;
    uint32_t visited;
};

struct it_ARRAY_DATA {
    uint64_t length;
    struct gc_OBJECT_REGISTRY* gc_registry_entry;
    struct ts_TYPE_ARRAY* type;
    union itval elements[1];
};

struct it_CLAZZ_DATA {
    struct ts_TYPE_CLAZZ* phi_table;
    struct gc_OBJECT_REGISTRY* gc_registry_entry;
    union itval itval[0];
};

struct it_OPCODE {
    uint8_t type;
    void* payload;
    uint32_t linenum;
};
struct it_STACKFRAME {
    union itval* registers;
    int registerc;
    int registers_allocated;
    uint32_t returnreg;
    struct it_OPCODE* iptr;
    struct it_METHOD* method;
    int index;
};
struct it_PROGRAM;
typedef union itval (*it_METHOD_REPLACEMENT_PTR)(struct it_PROGRAM*, struct it_STACKFRAME*, union itval*);
struct it_METHOD {
    uint32_t id;
    // This could technically be a uint8_t, but ehh
    int nargs;
    int opcodec;
    char* name;
    uint32_t registerc;
    struct it_OPCODE* opcodes;
    union ts_TYPE* returntype;
    // This is a pointer to an array
    union ts_TYPE** register_types;
    struct ts_TYPE_CLAZZ* containing_clazz;
    it_METHOD_REPLACEMENT_PTR replacement_ptr;
};
struct sv_STATIC_VAR {
    union ts_TYPE* type;
    char* name;
    union itval value;
};
struct it_PROGRAM {
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
    struct ts_TYPE_CLAZZ** clazzes;

    int static_varsc;
    // This is a pointer to the start of the array of static variables
    struct sv_STATIC_VAR* static_vars;
    int static_var_index;
};


#endif /* INTERPRETER_EXPORTS_H */