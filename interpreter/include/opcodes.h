#include <stdint.h>

#define SEGMENT_TYPE_METHOD 0x00
#define SEGMENT_TYPE_METADATA 0x01
#define SEGMENT_TYPE_CLASS 0x02

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
#define OPCODE_SEP 0x15
#define OPCODE_LOAD 0x16
#define OPCODE_LT 0x17

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
