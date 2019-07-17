#include <stdint.h>
#include "interpreter.h"

#ifndef OPCODES_H
#define OPCODES_H

#define SEGMENT_TYPE_METHOD 0x00
#define SEGMENT_TYPE_METADATA 0x01
#define SEGMENT_TYPE_CLASS 0x02
#define SEGMENT_TYPE_STATIC_VARIABLES 0x03

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

typedef struct {
    uint32_t target;
    uint64_t data;
} it_OPCODE_DATA_LOAD;
typedef struct {
    uint32_t target;
} it_OPCODE_DATA_ZERO;
typedef struct {
    uint32_t source1;
    uint32_t source2;
    uint32_t target;
} it_OPCODE_DATA_ADD;
typedef struct {
    uint32_t target;
} it_OPCODE_DATA_TWOCOMP;
typedef struct {
    uint32_t source1;
    uint32_t source2;
    uint32_t target;
} it_OPCODE_DATA_MULT;
typedef struct {
    uint32_t source1;
    uint32_t source2;
    uint32_t target;
} it_OPCODE_DATA_MODULO;
typedef struct {
    uint32_t callee;
    uint32_t returnval;
} it_OPCODE_DATA_CALL;
typedef struct {
    uint32_t target;
} it_OPCODE_DATA_RETURN;
typedef struct {
    uint32_t source1;
    uint32_t source2;
    uint32_t target;
} it_OPCODE_DATA_EQUALS;
typedef struct {
    uint32_t target;
} it_OPCODE_DATA_INVERT;
typedef struct {
    uint32_t source1;
    uint32_t source2;
    uint32_t target;
} it_OPCODE_DATA_LTEQ;
typedef struct {
    uint32_t source1;
    uint32_t source2;
    uint32_t target;
} it_OPCODE_DATA_GT;
typedef struct {
    uint32_t target;
} it_OPCODE_DATA_GOTO;
typedef struct {
    uint32_t predicate;
    uint32_t target;
} it_OPCODE_DATA_JF;
typedef struct {
    uint32_t source;
    uint8_t target;
} it_OPCODE_DATA_PARAM;
typedef struct {
    uint32_t source1;
    uint32_t source2;
    uint32_t target;
} it_OPCODE_DATA_GTEQ;
typedef struct {
    uint32_t source1;
    uint32_t source2;
    uint32_t target;
} it_OPCODE_DATA_XOR;
typedef struct {
    uint32_t source1;
    uint32_t source2;
    uint32_t target;
} it_OPCODE_DATA_AND;
typedef struct {
    uint32_t source1;
    uint32_t source2;
    uint32_t target;
} it_OPCODE_DATA_OR;
typedef struct {
    uint32_t source;
    uint32_t target;
} it_OPCODE_DATA_MOV;
typedef struct {
    uint32_t source1;
    uint32_t source2;
    uint32_t target;
} it_OPCODE_DATA_LT;
typedef struct {
    ts_TYPE_CLAZZ* clazz;
    uint32_t dest;
} it_OPCODE_DATA_NEW;
typedef struct {
    uint32_t clazzreg;
    uint32_t property_index;
    uint32_t destination;
} it_OPCODE_DATA_ACCESS;
typedef struct {
    uint32_t clazzreg;
    uint32_t property_index;
    uint32_t source;
} it_OPCODE_DATA_ASSIGN;
typedef struct {
    uint32_t targetreg;
    uint32_t callee_index;
    uint32_t returnreg;
} it_OPCODE_DATA_CLASSCALL;
typedef struct {
    uint32_t arrreg;
    uint32_t lengthreg;
} it_OPCODE_DATA_ARRALLOC;
typedef struct {
    uint32_t arrreg;
    uint32_t indexreg;
    uint32_t elementreg;
} it_OPCODE_DATA_ARRACCESS;
typedef struct {
    uint32_t arrreg;
    uint32_t indexreg;
    uint32_t elementreg;
} it_OPCODE_DATA_ARRASSIGN;
typedef struct {
    uint32_t arrreg;
    uint32_t resultreg;
} it_OPCODE_DATA_ARRLEN;
typedef struct {
    uint32_t source1;
    uint32_t source2;
    uint32_t target;
} it_OPCODE_DATA_DIV;
typedef struct {
    uint32_t source;
    uint32_t target;
    ts_TYPE* source_register_type;
    ts_TYPE* target_type;
} it_OPCODE_DATA_CAST;
typedef struct {
    uint32_t source;
    uint32_t destination;
    ts_TYPE* source_register_type;
    ts_TYPE* predicate_type;
} it_OPCODE_DATA_INSTANCEOF;
typedef struct {
    uint32_t source_var;
    uint32_t destination;
} it_OPCODE_DATA_STATICVARGET;
typedef struct {
    uint32_t source;
    uint32_t destination_var;
} it_OPCODE_DATA_STATICVARSET;

#endif
