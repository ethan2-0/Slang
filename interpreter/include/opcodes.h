#include <stdint.h>

#ifndef OPCODES_H
#define OPCODES_H

#define TS_MAX_TYPE_ARGS 16

struct it_METHOD;
struct ts_TYPE;

#define SEGMENT_TYPE_METHOD 0x00
#define SEGMENT_TYPE_METADATA 0x01
#define SEGMENT_TYPE_CLASS 0x02
#define SEGMENT_TYPE_STATIC_VARIABLES 0x03
#define SEGMENT_TYPE_INTERFACE 0x04

#define OPCODE_ZERO 0x01
#define OPCODE_ADD 0x02
#define OPCODE_TWOCOMP 0x03
#define OPCODE_MULT 0x04
#define OPCODE_MODULO 0x05
#define OPCODE_CALL 0x06
#define OPCODE_RETURN 0x07
#define OPCODE_EQUALS 0x08
#define OPCODE_INVERT 0x09
#define OPCODE_LTEQ 0x0a
#define OPCODE_GT 0x0b
#define OPCODE_GOTO 0x0c
#define OPCODE_JF 0x0d
#define OPCODE_PARAM 0x0e
#define OPCODE_GTEQ 0x0f
#define OPCODE_XOR 0x10
#define OPCODE_AND 0x11
#define OPCODE_OR 0x12
#define OPCODE_MOV 0x13
#define OPCODE_NOP 0x14
#define OPCODE_LOAD 0x16
#define OPCODE_LT 0x17
#define OPCODE_NEW 0x18
#define OPCODE_ACCESS 0x19
#define OPCODE_ASSIGN 0x1a
#define OPCODE_CLASSCALL 0x1b
#define OPCODE_ARRALLOC 0x1c
#define OPCODE_ARRACCESS 0x1d
#define OPCODE_ARRASSIGN 0x1e
#define OPCODE_ARRLEN 0x1f
#define OPCODE_DIV 0x20
#define OPCODE_CAST 0x21
#define OPCODE_INSTANCEOF 0x22
#define OPCODE_STATICVARGET 0x23
#define OPCODE_STATICVARSET 0x24
#define OPCODE_CLASSCALLSPECIAL 0x25
#define OPCODE_INTERFACECALL 0x26

struct it_OPCODE_DATA_LOAD {
    uint32_t target;
    uint64_t data;
};
struct it_OPCODE_DATA_ZERO {
    uint32_t target;
};
struct it_OPCODE_DATA_ADD {
    uint32_t source1;
    uint32_t source2;
    uint32_t target;
};
struct it_OPCODE_DATA_TWOCOMP {
    uint32_t target;
};
struct it_OPCODE_DATA_MULT {
    uint32_t source1;
    uint32_t source2;
    uint32_t target;
};
struct it_OPCODE_DATA_MODULO {
    uint32_t source1;
    uint32_t source2;
    uint32_t target;
};
struct it_OPCODE_DATA_CALL {
    struct it_METHOD* callee;
    uint32_t returnval;
    int type_paramsc;
    uint32_t type_params[TS_MAX_TYPE_ARGS];
};
struct it_OPCODE_DATA_RETURN {
    uint32_t target;
};
struct it_OPCODE_DATA_EQUALS {
    uint32_t source1;
    uint32_t source2;
    uint32_t target;
};
struct it_OPCODE_DATA_INVERT {
    uint32_t target;
};
struct it_OPCODE_DATA_LTEQ {
    uint32_t source1;
    uint32_t source2;
    uint32_t target;
};
struct it_OPCODE_DATA_GT {
    uint32_t source1;
    uint32_t source2;
    uint32_t target;
};
struct it_OPCODE_DATA_GOTO {
    uint32_t target;
};
struct it_OPCODE_DATA_JF {
    uint32_t predicate;
    uint32_t target;
};
struct it_OPCODE_DATA_PARAM {
    uint32_t source;
    uint8_t target;
};
struct it_OPCODE_DATA_GTEQ {
    uint32_t source1;
    uint32_t source2;
    uint32_t target;
};
struct it_OPCODE_DATA_XOR {
    uint32_t source1;
    uint32_t source2;
    uint32_t target;
};
struct it_OPCODE_DATA_AND {
    uint32_t source1;
    uint32_t source2;
    uint32_t target;
};
struct it_OPCODE_DATA_OR {
    uint32_t source1;
    uint32_t source2;
    uint32_t target;
};
struct it_OPCODE_DATA_MOV {
    uint32_t source;
    uint32_t target;
};
struct it_OPCODE_DATA_LT {
    uint32_t source1;
    uint32_t source2;
    uint32_t target;
};
struct it_OPCODE_DATA_NEW {
    struct ts_TYPE* clazz;
    uint32_t dest;
};
struct it_OPCODE_DATA_ACCESS {
    uint32_t clazzreg;
    uint32_t property_index;
    uint32_t destination;
};
struct it_OPCODE_DATA_ASSIGN {
    uint32_t clazzreg;
    uint32_t property_index;
    uint32_t source;
};
struct it_OPCODE_DATA_CLASSCALL {
    uint32_t targetreg;
    uint32_t callee_index;
    uint32_t returnreg;
};
struct it_OPCODE_DATA_ARRALLOC {
    uint32_t arrreg;
    uint32_t lengthreg;
};
struct it_OPCODE_DATA_ARRACCESS {
    uint32_t arrreg;
    uint32_t indexreg;
    uint32_t elementreg;
};
struct it_OPCODE_DATA_ARRASSIGN {
    uint32_t arrreg;
    uint32_t indexreg;
    uint32_t elementreg;
};
struct it_OPCODE_DATA_ARRLEN {
    uint32_t arrreg;
    uint32_t resultreg;
};
struct it_OPCODE_DATA_DIV {
    uint32_t source1;
    uint32_t source2;
    uint32_t target;
};
struct it_OPCODE_DATA_CAST {
    uint32_t source;
    uint32_t target;
};
struct it_OPCODE_DATA_INSTANCEOF {
    uint32_t source;
    uint32_t destination;
    uint32_t predicate_type_index;
};
struct it_OPCODE_DATA_STATICVARGET {
    uint32_t source_var;
    uint32_t destination;
};
struct it_OPCODE_DATA_STATICVARSET {
    uint32_t source;
    uint32_t destination_var;
};
struct it_OPCODE_DATA_CLASSCALLSPECIAL {
    struct it_METHOD* method;
    uint32_t destination_register;
    uint32_t callee_register;
};
struct it_OPCODE_DATA_INTERFACECALL {
    uint32_t method_index;
    uint32_t interface_id;
    uint32_t callee_register;
    uint32_t destination_register;
};

union it_OPCODE_DATA {
    struct it_OPCODE_DATA_LOAD load;
    struct it_OPCODE_DATA_ARRALLOC arralloc;
    struct it_OPCODE_DATA_ARRACCESS arraccess;
    struct it_OPCODE_DATA_ARRASSIGN arrassign;
    struct it_OPCODE_DATA_ARRLEN arrlen;
    struct it_OPCODE_DATA_ZERO zero;
    struct it_OPCODE_DATA_CLASSCALL classcall;
    struct it_OPCODE_DATA_INTERFACECALL interfacecall;
    struct it_OPCODE_DATA_CALL call;
    struct it_OPCODE_DATA_NEW new;
    struct it_OPCODE_DATA_ACCESS access;
    struct it_OPCODE_DATA_ASSIGN assign;
    struct it_OPCODE_DATA_PARAM param;
    struct it_OPCODE_DATA_RETURN return_;
    struct it_OPCODE_DATA_GOTO goto_;
    struct it_OPCODE_DATA_JF jf;
    struct it_OPCODE_DATA_EQUALS equals;
    struct it_OPCODE_DATA_LT lt;
    struct it_OPCODE_DATA_LTEQ lteq;
    struct it_OPCODE_DATA_GT gt;
    struct it_OPCODE_DATA_GTEQ gteq;
    struct it_OPCODE_DATA_ADD add;
    struct it_OPCODE_DATA_TWOCOMP twocomp;
    struct it_OPCODE_DATA_MULT mult;
    struct it_OPCODE_DATA_DIV div;
    struct it_OPCODE_DATA_CAST cast;
    struct it_OPCODE_DATA_INSTANCEOF instanceof;
    struct it_OPCODE_DATA_STATICVARGET staticvarget;
    struct it_OPCODE_DATA_STATICVARSET staticvarset;
    struct it_OPCODE_DATA_CLASSCALLSPECIAL classcallspecial;
    struct it_OPCODE_DATA_MODULO modulo;
    struct it_OPCODE_DATA_XOR xor;
    struct it_OPCODE_DATA_AND and;
    struct it_OPCODE_DATA_OR or;
    struct it_OPCODE_DATA_INVERT invert;
    struct it_OPCODE_DATA_MOV mov;
};

#endif /* OPCODES_H */
