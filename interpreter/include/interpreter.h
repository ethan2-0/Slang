#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

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
bool ts_is_compatible(ts_TYPE* type1, ts_TYPE* type2);
bool ts_instanceof(ts_TYPE* lhs, ts_TYPE* rhs);

struct it_CLAZZ_DATA;
struct it_ARRAY_DATA;

union itval {
    int64_t number;
    struct it_CLAZZ_DATA* clazz_data;
    struct it_ARRAY_DATA* array_data;
};

typedef struct gc_OBJECT_REGISTRY {
    struct gc_OBJECT_REGISTRY* next;
    struct gc_OBJECT_REGISTRY* previous;
    uint64_t object_id;
    bool is_present;
    size_t allocation_size;
    union itval object;
    ts_CATEGORY category;
    uint32_t visited;
} gc_OBJECT_REGISTRY;

struct it_ARRAY_DATA {
    uint64_t length;
    gc_OBJECT_REGISTRY* gc_registry_entry;
    ts_TYPE_ARRAY* type;
    union itval elements[1];
};

struct it_CLAZZ_DATA {
    ts_TYPE_CLAZZ* phi_table;
    gc_OBJECT_REGISTRY* gc_registry_entry;
    union itval itval[0];
};

typedef union itval itval;

typedef struct {
    uint8_t type;
    void* payload;
    uint32_t linenum;
} it_OPCODE;
typedef struct {
    itval* registers;
    int registerc;
    int registers_allocated;
    uint32_t returnreg;
    it_OPCODE* iptr;
    struct it_METHOD* method;
    int index;
} it_STACKFRAME;
typedef itval (*it_METHOD_REPLACEMENT_PTR)(it_STACKFRAME*, itval*);
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
    ts_TYPE** register_types;
    ts_TYPE_CLAZZ* containing_clazz;
    it_METHOD_REPLACEMENT_PTR replacement_ptr;
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

void it_run(it_PROGRAM* prog);
void it_replace_methods(it_PROGRAM* prog);

void cl_arrange_phi_tables(it_PROGRAM* program);

gc_OBJECT_REGISTRY* gc_register_object(itval object, size_t allocation_size, ts_CATEGORY category);
void gc_collect(it_STACKFRAME* stack, it_STACKFRAME* current_frame);
bool gc_needs_collection;

#define NUM_REPLACED_METHODS 4

#endif
