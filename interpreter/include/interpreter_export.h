#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef INTERPRETER_EXPORT_H
#define INTERPRETER_EXPORT_H

#define TS_MAX_TYPE_ARGS 16
#define CATEGORY_GUARDS 0
#define CATEGORY_GUARDS_EVERY_ITERATION 0

#include "opcodes.h"

struct ts_TYPE;
struct it_METHOD;

enum ts_CATEGORY {
    ts_CATEGORY_PRIMITIVE,
    ts_CATEGORY_CLAZZ,
    ts_CATEGORY_ARRAY,
    ts_CATEGORY_INTERFACE,
    ts_CATEGORY_TYPE_PARAMETER
};
struct ts_CLAZZ_FIELD {
    char* name;
    struct ts_TYPE* type;
};
struct it_METHOD_TABLE;
struct ts_TYPE_ARGUMENTS;
struct ts_TYPE_CLAZZ {
    struct ts_TYPE* immediate_supertype;
    int nfields;
    struct ts_CLAZZ_FIELD* fields;
    // This is a pointer to an array of pointers
    struct it_METHOD_TABLE* method_table;
    int implemented_interfacesc;
    // This is a pointer to an array of pointers
    struct ts_TYPE** implemented_interfaces;
    struct ts_GENERIC_TYPE_CONTEXT* type_parameters;
    struct ts_TYPE* specialized_from;
    struct ts_TYPE_ARGUMENTS* type_arguments;
    struct ts_TYPE** specializations;
    int specializationsc;
    int specializations_size;
};
struct ts_TYPE_ARRAY {
    struct ts_TYPE* parent_type;
};
struct ts_TYPE_INTERFACE {
    // Note that this is only used to keep references to the methods, not for lookup at runtime
    struct it_METHOD_TABLE* methods;
};
struct ts_TYPE_PARAMETER {
    struct ts_GENERIC_TYPE_CONTEXT* context;
    struct ts_TYPE* extends;
    int implementsc;
    struct ts_TYPE** implements;
};
struct ts_GENERIC_TYPE_CONTEXT;
struct ts_TYPE {
    enum ts_CATEGORY category;
    uint32_t id;
    char* name;
    int heirarchy_len;
    // This is a pointer to an array of pointers
    struct ts_TYPE** heirarchy;
    union {
        struct ts_TYPE_CLAZZ clazz;
        struct ts_TYPE_ARRAY array;
        struct ts_TYPE_INTERFACE interface;
        struct ts_TYPE_PARAMETER type_parameter;
    } data;
};
struct ts_GENERIC_TYPE_CONTEXT {
    struct ts_GENERIC_TYPE_CONTEXT* parent;
    uint32_t count;
    struct ts_TYPE* arguments[];
};
struct ts_TYPE_ARGUMENTS {
    struct ts_GENERIC_TYPE_CONTEXT* context;
    struct {
        struct ts_TYPE* key;
        struct ts_TYPE* value;
    } mapping[];
};

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
    #if CATEGORY_GUARDS
    enum ts_CATEGORY category;
    #endif
    uint64_t length;
    struct gc_OBJECT_REGISTRY* gc_registry_entry;
    struct ts_TYPE* type;
    union itval elements[1];
};

struct it_INTERFACE_IMPLEMENTATION {
    struct it_METHOD_TABLE* method_table;
    uint32_t interface_id;
};
struct it_METHOD_TABLE {
    enum ts_CATEGORY category;
    struct ts_TYPE* type;
    uint32_t ninterface_implementations;
    // This is a pointer to an array of pointers
    struct it_INTERFACE_IMPLEMENTATION* interface_implementations;
    uint32_t nmethods;
    struct it_METHOD* methods[];
};

struct it_CLAZZ_DATA {
    #if CATEGORY_GUARDS
    enum ts_CATEGORY category;
    #endif
    struct it_METHOD_TABLE* method_table;
    struct gc_OBJECT_REGISTRY* gc_registry_entry;
    union itval itval[0];
};

struct it_OPCODE {
    uint8_t type;
    uint32_t linenum;
    union it_OPCODE_DATA data;
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
    struct ts_TYPE* returntype;
    // This is a pointer to an array
    struct ts_TYPE** register_types;
    struct ts_TYPE* containing_clazz;
    struct ts_TYPE* containing_interface;
    it_METHOD_REPLACEMENT_PTR replacement_ptr;
    uint32_t typereferencec;
    struct ts_TYPE** typereferences;
    struct ts_GENERIC_TYPE_CONTEXT* type_parameters;
    // This is a pointer to an array of pointers
    struct it_METHOD** reifications;
    int reificationsc;
    int reifications_size;
    struct ts_TYPE_ARGUMENTS* typeargs;
    struct it_METHOD* reified_from;
    // This is set to true in ts_reify_generic_references(..)
    bool has_had_references_reified;
};
struct sv_STATIC_VAR {
    struct ts_TYPE* type;
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
    struct ts_TYPE** clazzes;
    int static_varsc;
    int static_var_index;
    // This is a pointer to the start of the array of static variables
    struct sv_STATIC_VAR* static_vars;
    int interfacesc;
    int interface_index;
    struct ts_TYPE** interfaces;
};


#endif /* INTERPRETER_EXPORT_H */