#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef INTERPRETER_EXPORTS_H
#define INTERPRETER_EXPORTS_H

#define TS_MAX_TYPE_ARGS 16

union ts_TYPE;
struct it_METHOD;

enum ts_CATEGORY {
    ts_CATEGORY_PRIMITIVE,
    ts_CATEGORY_CLAZZ,
    ts_CATEGORY_ARRAY,
    ts_CATEGORY_INTERFACE,
    ts_CATEGORY_TYPE_PARAMETER
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
struct it_METHOD_TABLE;
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
    struct it_METHOD_TABLE* method_table;
    int implemented_interfacesc;
    // This is a pointer to an array of pointers
    struct ts_TYPE_INTERFACE** implemented_interfaces;
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
struct ts_TYPE_INTERFACE {
    enum ts_CATEGORY category;
    uint32_t id;
    char* name;
    int heirarchy_len;
    union ts_TYPE** heirarchy;
    // Note that this is only used to keep references to the methods, not for lookup at runtime
    struct it_METHOD_TABLE* methods;
};
struct ts_TYPE_PARAMETER {
    enum ts_CATEGORY category;
    uint32_t id;
    char* name;
    int heirarchy_len;
    union ts_TYPE** heirarchy;
};
union ts_TYPE {
    struct ts_TYPE_BAREBONES barebones;
    struct ts_TYPE_CLAZZ clazz;
    struct ts_TYPE_ARRAY array;
    struct ts_TYPE_INTERFACE interface;
    struct ts_TYPE_PARAMETER type_parameter;
};
struct ts_GENERIC_TYPE_CONTEXT {
    struct ts_GENERIC_TYPE_CONTEXT* parent;
    uint32_t count;
    struct ts_TYPE_PARAMETER arguments[];
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

struct it_INTERFACE_IMPLEMENTATION {
    struct it_METHOD_TABLE* method_table;
    uint32_t interface_id;
};
struct it_METHOD_TABLE {
    enum ts_CATEGORY category;
    union ts_TYPE* type;
    uint32_t ninterface_implementations;
    // This is a pointer to an array of pointers
    struct it_INTERFACE_IMPLEMENTATION* interface_implementations;
    uint32_t nmethods;
    struct it_METHOD* methods[];
};

struct it_CLAZZ_DATA {
    struct it_METHOD_TABLE* method_table;
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
    struct ts_TYPE_INTERFACE* containing_interface;
    it_METHOD_REPLACEMENT_PTR replacement_ptr;
    uint32_t typereferencec;
    union ts_TYPE** typereferences;
    struct ts_GENERIC_TYPE_CONTEXT* type_parameters;
    // This is a pointer to an array of pointers
    struct it_METHOD** reifications;
    int reificationsc;
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
    int static_var_index;
    // This is a pointer to the start of the array of static variables
    struct sv_STATIC_VAR* static_vars;
    int interfacesc;
    int interface_index;
    struct ts_TYPE_INTERFACE** interfaces;
};


#endif /* INTERPRETER_EXPORTS_H */