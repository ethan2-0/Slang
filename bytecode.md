This is a brief specification for the bytecode format and instruction set of the
instruction set used in this programming language.

# Overview

## Calling Convention

Arguments go in order in the first registers of the method being called. Use
the `PARAM` instruction to pass parameters.

## File format

The file begins with the four bytes `0xcf 70 2b 56`, followed by zero or more
segments.

## Segments

Each segment has a 1-byte type, followed by a 4-byte length.

**Segment type `0x00` `METHOD`**

METHOD segments describe a method. It begins with a 4-byte ID. It is followed by
a sequence of opcodes.

## Opcodes

An opcode comprises a 1-byte identifier, followed by a series of arguments, each
1, 4, 8, or variable bytes in size. Variable-sized arguments are preceded by
four bytes encoding their length, and are specified with a `v` in the argument
format specifier.

### Basic

**Opcode `0x14` `NOP`, accepting none**

Do nothing.

**Opcode `0x00` `LOAD`, accepting 4-8**

Load value x1 into register x0.

**Opcode `0x01` `ZERO`, accepting 4**

Set the value in register x0 to `0x00 00 00 00`.

**Opcode `0x06` `CALL`, accepting v-4**

Calls the method with name x0 using the parameters set up using the opcode
`PARAM`, and puts the return value into register x1. Clears the list of method
parameters.

**Opcode `0x0e` `PARAM`, accepting 4-1**

Adds x0 to the x1th element in the list of method parameters. Note that inserting
instructions other than `PARAM` between `PARAM` and `CALL` instructions may
result in undefined behavior.

**Opcode `0x07` `RETURN`, accepting 4**

Returns from the current method, passing the value in register x0 as the return
value.

### Control Flow

**Opcode `0x0c` `GOTO`, accepting 4**

Jump to the opcode number x0 within the current method.

**Opcode `0x0d` `CJ`, accepting 4-4**

If the value in register x0 is not exactly equal to `0x00 00 00 00`, then jump
to the opcode number x1 within the current method.

### Comparison

**Opcode `0x08` `EQUALS`, accepting 4-4-4**

If the value in register x0 is exactly equal to the value in register x1, then
set the value in register x2 to `0x00 00 00 01`. Otherwise, set the value in
register x2 to `0x00 00 00 00`.

**Opcode `0x09` `LT`, accepting 4-4-4**

If the value in register x0 is less than the value in register x1, then
set the value in register x2 to `0x00 00 00 01`. Otherwise, set the value in
register x2 to `0x00 00 00 00`.

**Opcode `0x0a` `LTEQ`, accepting 4-4-4**

If the value in register x0 is less than or equal to the value in register x1,
then set the value in register x2 to `0x00 00 00 01`. Otherwise, set the value
in register x2 to `0x00 00 00 00`.

**Opcode `0x0b` `GT`, accepting 4-4-4**

If the value in register x0 is greater than the value in register x1, then
set the value in register x2 to `0x00 00 00 01`. Otherwise, set the value in
register x2 to `0x00 00 00 00`.

**Opcode `0x0f` `GTEQ`, accepting 4-4-4**

If the value in register x0 is greater than or equal to the value in register
x1, then set the value in register x2 to `0x00 00 00 01`. Otherwise, set the
value in register x2 to `0x00 00 00 00`.

### Arithmetic & Bitwise

**Opcode `0x02` `ADD`, accepting 4-4-4**

Add the value in register x0 and register x1 and put it into register x2.

**Opcode `0x03` `TWOCOMP`, accepting 4**

Set the value in the register x0 to the two's complement of the value in the
register x0.

**Opcode `0x04` `MULT`, accepting 4-4-4**

Multiply the values in register x0 and x1 and put it into register x2.

**Opcode `0x05` `MODULO`, accepting 4-4-4**

Set the value in register x2 to the value in register x0 modulo the value in x1.

**Opcode `0x10` `XOR`, accepting 4-4-4**

Set the value in register x2 to the value in register x0 bitwise XOR the value
in x1.

**Opcode `0x11` `AND`, accepting 4-4-4**

Set the value in register x2 to the value in register x0 bitwise AND the value
in x1.

**Opcode `0x12` `OR`, accepting 4-4-4**

Set the value in register x2 to the value in register x0 bitwise OR the value in
x1.

**Opcode `0x09` `INVERT`, accepting 4**

If the value in register x0 is precisely equal to `0x00 00 00 00`, then set
the value in register x0 to `0x00 00 00 01`. Otherwise, set the value in
register x0 to `0x00 00 00 00`.

**Opcode `0x13` `MOV`, accepting 4-4**

Set the value in register x1 to the value in register x0.
