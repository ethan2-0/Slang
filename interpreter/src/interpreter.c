#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "common.h"
#include "interpreter.h"
#include "opcodes.h"

#define STACKSIZE 1024

extern bool gc_needs_collection;

void it_traceback(it_STACKFRAME* stackptr) {
    printf("Traceback (most recent call first):\n");
    while(true) {
        if(stackptr->method->containing_clazz != NULL) {
            printf("Method %s, of class %s, line %d\n", stackptr->method->name, stackptr->method->containing_clazz->name, stackptr->iptr->linenum);
        } else {
            printf("Method %s, line %d\n", stackptr->method->name, stackptr->iptr->linenum);
        }

        if(stackptr->index <= 0) {
            break;
        }
        stackptr--;
    }
}
void load_method(it_STACKFRAME* stackptr, it_METHOD* method) {
    stackptr->method = method;
    stackptr->iptr = stackptr->method->opcodes;
    stackptr->registerc = stackptr->method->registerc;
    if(stackptr->registers_allocated < stackptr->registerc) {
        #if DEBUG
        printf("Allocating more register space: %d < %d\n", stackptr->registers_allocated, stackptr->registerc);
        #endif
        free(stackptr->registers);
        stackptr->registers = mm_malloc(sizeof(itval) * stackptr->registerc);
    }
    // Technically, we've already zeroed out the memory (in mm_malloc(...)).
    // But that's mostly to ease debugging, I don't want to rely on it.
    memset(stackptr->registers, 0x00, stackptr->registerc * sizeof(itval));
}
void it_execute(it_PROGRAM* prog, it_OPTIONS* options) {
    // Setup interpreter state
    it_STACKFRAME* stack = mm_malloc(sizeof(it_STACKFRAME) * STACKSIZE);
    it_STACKFRAME* stackptr = stack;
    it_STACKFRAME* stackend = stack + STACKSIZE;
    for(int i = 0; i < STACKSIZE; i++) {
        stack[i].registers_allocated = 0;
        // Set the register file pointer to the null pointer so we can free() it.
        stack[i].registers = 0;
        stack[i].index = i;
    }
    load_method(stackptr, &prog->methods[prog->entrypoint]);
    itval* registers = stackptr->registers;
    #if DEBUG
    printf("About to execute method %d\n", stackptr->method->id);
    #endif
    // This is the start of the instruction tape
    it_OPCODE* instruction_start = stackptr->iptr;
    // This is the current index into the instruction tape
    it_OPCODE* iptr = instruction_start;
    // See the documentation for PARAM and CALL for details.
    // Note that this is preserved globally across method calls.
    itval params[256];
    memset(params, 0, sizeof(itval) * 256);

    // TODO: Do something smarter for dispatch here
    while(1) {
        #if DEBUG
        for(int i = 0; i < stackptr->registerc; i++) {
            printf("%02x ", registers[i]);
        }
        printf("\n");
        printf("Type %02x, iptr %d\n", iptr->type, iptr - instruction_start);
        #endif
        if(gc_needs_collection) {
            gc_collect(stack, stackptr);
        }
        switch(iptr->type) {
        case OPCODE_PARAM:
            params[((it_OPCODE_DATA_PARAM*) iptr->payload)->target] = registers[((it_OPCODE_DATA_PARAM*) iptr->payload)->source];
            iptr++;
            continue;
        case OPCODE_CALL:
            ; // Labels can't be immediately followed by declarations, because
              // C is a barbaric language
            uint32_t callee_id = ((it_OPCODE_DATA_CALL*) iptr->payload)->callee;
            it_METHOD* callee = &prog->methods[callee_id];
            if(callee->replacement_ptr != NULL) {
                #if DEBUG
                printf("Calling replaced method %s\n", callee->name);
                #endif
                stackptr->iptr = iptr;
                uint32_t returnreg = ((it_OPCODE_DATA_CALL*) iptr->payload)->returnval;
                registers[returnreg] = callee->replacement_ptr(stackptr, params);
                iptr++;
                continue;
            }
            it_STACKFRAME* oldstack = stackptr;
            stackptr++;
            if(stackptr >= stackend) {
                stackptr--;
                it_traceback(stackptr);
                fatal("Stack overflow");
            }

            #if DEBUG
            printf("Calling %d\n", callee_id);
            #endif

            oldstack->returnreg = ((it_OPCODE_DATA_CALL*) iptr->payload)->returnval;

            #if DEBUG
            printf("Need %d args.\n", callee->nargs);
            for(uint32_t i = 0; i < callee->nargs; i++) {
                printf("Argument: %02x\n", params[i]);
            }
            #endif

            load_method(stackptr, callee);
            registers = stackptr->registers;
            instruction_start = stackptr->iptr;
            oldstack->iptr = iptr;
            iptr = instruction_start;
            for(uint32_t i = 0; i < callee->nargs; i++) {
                registers[i] = params[i];
            }
            continue;
        case OPCODE_CLASSCALL:
            ;
            it_STACKFRAME* classcall_oldstack = stackptr;
            stackptr++;
            if(stackptr >= stackend) {
                stackptr--;
                it_traceback(stackptr);
                fatal("Stack overflow");
            }

            uint32_t classcall_callee_index = ((it_OPCODE_DATA_CLASSCALL*) iptr->payload)->callee_index;

            classcall_oldstack->returnreg = ((it_OPCODE_DATA_CLASSCALL*) iptr->payload)->returnreg;
            itval thiz = registers[((it_OPCODE_DATA_CLASSCALL*) iptr->payload)->targetreg];
            it_METHOD* classcall_callee = thiz.clazz_data->phi_table->methods[classcall_callee_index];

            #if DEBUG
            printf("Class-calling %s\n", classcall_callee->name);
            printf("Need %d args, incl this.\n", classcall_callee->nargs);
            printf("This: %02x\n", thiz);
            for(uint8_t i = 0; i < classcall_callee->nargs - 1; i++) {
                printf("Argument: %02x\n", params[i + 1]);
            }
            #endif

            load_method(stackptr, classcall_callee);
            registers = stackptr->registers;
            instruction_start = stackptr->iptr;
            classcall_oldstack->iptr = iptr;
            iptr = instruction_start;
            registers[0] = thiz;
            for(uint32_t i = 0; i < classcall_callee->nargs - 1; i++) {
                registers[i + 1] = params[i];
            }
            continue;
        case OPCODE_RETURN:
            ; // Labels can't be immediately followed by declarations, because
              // C is a barbaric language
            itval result = registers[((it_OPCODE_DATA_RETURN*) iptr->payload)->target];
            if(stackptr <= stack) {
                if(options->print_return_value) {
                    printf("Returned 0x%08lx\n", result.number);
                }
                goto CLEANUP;
            }
            #if DEBUG
            printf("Returning\n");
            #endif
            stackptr--;
            registers = stackptr->registers;
            instruction_start = stackptr->method->opcodes;
            iptr = stackptr->iptr;
            registers[stackptr->returnreg] = result;
            iptr++;
            continue;
        case OPCODE_ZERO:
            registers[((it_OPCODE_DATA_ZERO*) iptr->payload)->target].number = 0x00000000;
            iptr++;
            continue;
        case OPCODE_ADD:
            registers[((it_OPCODE_DATA_ADD*) iptr->payload)->target].number = registers[((it_OPCODE_DATA_ADD*) iptr->payload)->source1].number + registers[((it_OPCODE_DATA_ADD*) iptr->payload)->source2].number;
            iptr++;
            continue;
        case OPCODE_TWOCOMP:
            registers[((it_OPCODE_DATA_TWOCOMP*) iptr->payload)->target].number = -registers[((it_OPCODE_DATA_TWOCOMP*) iptr->payload)->target].number;
            iptr++;
            continue;
        case OPCODE_MULT:
            registers[((it_OPCODE_DATA_MULT*) iptr->payload)->target].number = registers[((it_OPCODE_DATA_MULT*) iptr->payload)->source1].number * registers[((it_OPCODE_DATA_MULT*) iptr->payload)->source2].number;
            iptr++;
            continue;
        case OPCODE_MODULO:
            registers[((it_OPCODE_DATA_MODULO*) iptr->payload)->target].number = registers[((it_OPCODE_DATA_MODULO*) iptr->payload)->source1].number % registers[((it_OPCODE_DATA_MODULO*) iptr->payload)->source2].number;
            iptr++;
            continue;
        case OPCODE_DIV:
            registers[((it_OPCODE_DATA_MODULO*) iptr->payload)->target].number = registers[((it_OPCODE_DATA_MODULO*) iptr->payload)->source1].number / registers[((it_OPCODE_DATA_MODULO*) iptr->payload)->source2].number;
            iptr++;
            continue;
        case OPCODE_EQUALS:
            // TODO: Make this work on systems where sizeof(itval.number) != sizeof(itval.itval)
            //       This probably involves a specialized EQUALS opcode, either in the compiler or
            //       just specializing in bytecode.c.
            if(registers[((it_OPCODE_DATA_EQUALS*) iptr->payload)->source1].number == registers[((it_OPCODE_DATA_EQUALS*) iptr->payload)->source2].number) {
                registers[((it_OPCODE_DATA_EQUALS*) iptr->payload)->target].number = 0x1;
            } else {
                registers[((it_OPCODE_DATA_EQUALS*) iptr->payload)->target].number = 0x0;
            }
            iptr++;
            continue;
        case OPCODE_INVERT:
            if(registers[((it_OPCODE_DATA_INVERT*) iptr->payload)->target].number == 0x0) {
                registers[((it_OPCODE_DATA_INVERT*) iptr->payload)->target].number = 0x1;
            } else {
                registers[((it_OPCODE_DATA_INVERT*) iptr->payload)->target].number = 0x0;
            }
            iptr++;
            continue;
        case OPCODE_LTEQ:
            if(registers[((it_OPCODE_DATA_LTEQ*) iptr->payload)->source1].number <= registers[((it_OPCODE_DATA_LTEQ*) iptr->payload)->source2].number) {
                registers[((it_OPCODE_DATA_LTEQ*) iptr->payload)->target].number = 0x1;
            } else {
                registers[((it_OPCODE_DATA_LTEQ*) iptr->payload)->target].number = 0x0;
            }
            iptr++;
            continue;
        case OPCODE_GT:
            if(registers[((it_OPCODE_DATA_GT*) iptr->payload)->source1].number > registers[((it_OPCODE_DATA_GT*) iptr->payload)->source2].number) {
                registers[((it_OPCODE_DATA_GT*) iptr->payload)->target].number = 0x1;
            } else {
                registers[((it_OPCODE_DATA_GT*) iptr->payload)->target].number = 0x0;
            }
            iptr++;
            continue;
        case OPCODE_GOTO:
            #if DEBUG
            printf("Jumping to %d\n", ((it_OPCODE_DATA_GOTO*) iptr->payload)->target);
            #endif
            iptr = instruction_start + ((it_OPCODE_DATA_GOTO*) iptr->payload)->target;
            continue;
        case OPCODE_JF:
            // JF stands for Jump if False.
            if(registers[((it_OPCODE_DATA_JF*) iptr->payload)->predicate].number == 0x00) {
                #if DEBUG
                printf("Jumping to %d\n", ((it_OPCODE_DATA_JF*) iptr->payload)->target);
                #endif
                iptr = instruction_start + ((it_OPCODE_DATA_JF*) iptr->payload)->target;
            } else {
                iptr++;
            }
            continue;
        case OPCODE_GTEQ:
            if(registers[((it_OPCODE_DATA_GTEQ*) iptr->payload)->source1].number >= registers[((it_OPCODE_DATA_GTEQ*) iptr->payload)->source2].number) {
                registers[((it_OPCODE_DATA_GTEQ*) iptr->payload)->target].number = 0x1;
            } else {
                registers[((it_OPCODE_DATA_GTEQ*) iptr->payload)->target].number = 0x0;
            }
            iptr++;
            continue;
        case OPCODE_XOR:
            registers[((it_OPCODE_DATA_XOR*) iptr->payload)->target].number = registers[((it_OPCODE_DATA_XOR*) iptr->payload)->source1].number ^ registers[((it_OPCODE_DATA_XOR*) iptr->payload)->source2].number;
            iptr++;
            continue;
        case OPCODE_AND:
            registers[((it_OPCODE_DATA_AND*) iptr->payload)->target].number = registers[((it_OPCODE_DATA_AND*) iptr->payload)->source1].number & registers[((it_OPCODE_DATA_AND*) iptr->payload)->source2].number;
            iptr++;
            continue;
        case OPCODE_OR:
            registers[((it_OPCODE_DATA_OR*) iptr->payload)->target].number = registers[((it_OPCODE_DATA_OR*) iptr->payload)->source1].number | registers[((it_OPCODE_DATA_OR*) iptr->payload)->source2].number;
            iptr++;
            continue;
        case OPCODE_MOV:
            registers[((it_OPCODE_DATA_MOV*) iptr->payload)->target] = registers[((it_OPCODE_DATA_MOV*) iptr->payload)->source];
            iptr++;
            continue;
        case OPCODE_NOP:
            // Do nothing
            iptr++;
            continue;
        case OPCODE_LOAD:
            registers[((it_OPCODE_DATA_LOAD*) iptr->payload)->target].number = ((it_OPCODE_DATA_LOAD*) iptr->payload)->data;
            iptr++;
            continue;
        case OPCODE_LT:
            if(registers[((it_OPCODE_DATA_LT*) iptr->payload)->source1].number < registers[((it_OPCODE_DATA_LT*) iptr->payload)->source2].number) {
                registers[((it_OPCODE_DATA_LT*) iptr->payload)->target].number = 0x1;
            } else {
                registers[((it_OPCODE_DATA_LT*) iptr->payload)->target].number = 0x0;
            }
            iptr++;
            continue;
        case OPCODE_NEW:
            ;
            it_OPCODE_DATA_NEW* opcode_new_data = (it_OPCODE_DATA_NEW*) iptr->payload;
            size_t new_allocation_size = sizeof(struct it_CLAZZ_DATA) + sizeof(itval) * opcode_new_data->clazz->nfields;
            registers[opcode_new_data->dest].clazz_data = mm_malloc(new_allocation_size);
            gc_OBJECT_REGISTRY* new_register = gc_register_object(registers[opcode_new_data->dest], new_allocation_size, ts_CATEGORY_CLAZZ);
            registers[opcode_new_data->dest].clazz_data->gc_registry_entry = new_register;
            memset(registers[opcode_new_data->dest].clazz_data->itval, 0, sizeof(itval) * opcode_new_data->clazz->nfields);
            registers[opcode_new_data->dest].clazz_data->phi_table = opcode_new_data->clazz;
            iptr++;
            continue;
        case OPCODE_ACCESS:
            ;
            it_OPCODE_DATA_ACCESS* opcode_access_data = (it_OPCODE_DATA_ACCESS*) iptr->payload;
            if(registers[opcode_access_data->clazzreg].clazz_data == NULL) {
                it_traceback(stackptr);
                fatal("Null pointer on access");
            }
            struct it_CLAZZ_DATA* clazz_data = registers[opcode_access_data->clazzreg].clazz_data;
            registers[opcode_access_data->destination] = clazz_data->itval[opcode_access_data->property_index];
            iptr++;
            continue;
        case OPCODE_ASSIGN:
            ;
            it_OPCODE_DATA_ASSIGN* opcode_assign_data = (it_OPCODE_DATA_ASSIGN*) iptr->payload;
            if(registers[opcode_assign_data->clazzreg].clazz_data == NULL) {
                it_traceback(stackptr);
                fatal("Null pointer on access");
            }
            registers[opcode_assign_data->clazzreg].clazz_data->itval[opcode_assign_data->property_index] = registers[opcode_assign_data->source];
            iptr++;
            continue;
        case OPCODE_ARRALLOC:
            ;
            it_OPCODE_DATA_ARRALLOC* opcode_arralloc_data = (it_OPCODE_DATA_ARRALLOC*) iptr->payload;
            uint64_t length = registers[opcode_arralloc_data->lengthreg].number;
            size_t arralloc_allocation_size = sizeof(it_ARRAY_DATA) + sizeof(itval) * (length <= 0 ? 1 : length - 1);
            registers[opcode_arralloc_data->arrreg].array_data = (it_ARRAY_DATA*) mm_malloc(arralloc_allocation_size);
            registers[opcode_arralloc_data->arrreg].array_data->length = length;
            memset(&registers[opcode_arralloc_data->arrreg].array_data->elements, 0, length * sizeof(itval));
            registers[opcode_arralloc_data->arrreg].array_data->type = (ts_TYPE_ARRAY*) stackptr->method->register_types[opcode_arralloc_data->arrreg];
            gc_OBJECT_REGISTRY* arralloc_registry = gc_register_object(registers[opcode_arralloc_data->arrreg], arralloc_allocation_size, ts_CATEGORY_ARRAY);
            registers[opcode_arralloc_data->arrreg].array_data->gc_registry_entry = arralloc_registry;
            iptr++;
            continue;
        case OPCODE_ARRACCESS:
            ;
            it_OPCODE_DATA_ARRACCESS* opcode_arraccess_data = (it_OPCODE_DATA_ARRACCESS*) iptr->payload;
            if(registers[opcode_arraccess_data->arrreg].array_data == NULL) {
                it_traceback(stackptr);
                fatal("Attempt to access null array");
            }
            if(registers[opcode_arraccess_data->indexreg].number >= registers[opcode_arraccess_data->arrreg].array_data->length) {
                it_traceback(stackptr);
                fatal("Array index out of bounds");
            }
            registers[opcode_arraccess_data->elementreg] = registers[opcode_arraccess_data->arrreg].array_data->elements[registers[opcode_arraccess_data->indexreg].number];
            iptr++;
            continue;
        case OPCODE_ARRASSIGN:
            ;
            it_OPCODE_DATA_ARRASSIGN* opcode_arrassign_data = (it_OPCODE_DATA_ARRASSIGN*) iptr->payload;
            if(registers[opcode_arrassign_data->arrreg].array_data == NULL) {
                it_traceback(stackptr);
                fatal("Attempt to assign to null array");
            }
            if(registers[opcode_arrassign_data->indexreg].number >= registers[opcode_arrassign_data->arrreg].array_data->length) {
                it_traceback(stackptr);
                fatal("Array index out of bounds");
            }
            registers[opcode_arrassign_data->arrreg].array_data->elements[registers[opcode_arrassign_data->indexreg].number] = registers[opcode_arrassign_data->elementreg];
            iptr++;
            continue;
        case OPCODE_ARRLEN:
            ;
            it_OPCODE_DATA_ARRLEN* opcode_arrlen_data = (it_OPCODE_DATA_ARRLEN*) iptr->payload;
            if(registers[opcode_arrlen_data->arrreg].array_data == NULL) {
                it_traceback(stackptr);
                fatal("Attempt to find length of null");
            }
            registers[opcode_arrlen_data->resultreg].number = registers[opcode_arrlen_data->arrreg].array_data->length;
            iptr++;
            continue;
        case OPCODE_CAST:
            {
                it_OPCODE_DATA_CAST* data = (it_OPCODE_DATA_CAST*) iptr->payload;
                itval source = registers[data->source];
                // We know data->source_register_type->barebones.category == ts_CATEGORY_CLAZZ
                if(!ts_is_compatible((ts_TYPE*) source.clazz_data->phi_table, data->target_type)) {
                    it_traceback(stackptr);
                    fatal("Incompatible cast");
                }
                registers[data->target] = source;
                iptr++;
                continue;
            }
        case OPCODE_INSTANCEOF:
            {
                it_OPCODE_DATA_INSTANCEOF* data = (it_OPCODE_DATA_INSTANCEOF*) iptr->payload;
                itval source = registers[data->source];
                registers[data->destination] = (itval) ((int64_t) ts_instanceof((ts_TYPE*) source.clazz_data->phi_table, data->predicate_type));
                iptr++;
                continue;
            }
        default:
            it_traceback(stackptr);
            fatal("Unexpected opcode");
        }
    }
CLEANUP:
    free(stack);
    // Explicitly not freeing stackptr and stackend since they're dangling pointers at this point
    return;
}
void it_run(it_PROGRAM* prog, it_OPTIONS* options) {
    #if DEBUG
    printf("Entering it_RUN\n");
    #endif
    it_execute(prog, options);
}
itval rm_print(it_STACKFRAME* stackptr, itval* params) {
    it_ARRAY_DATA* arr = params[0].array_data;
    char* chars = mm_malloc(sizeof(char) * (arr->length + 1));
    for(int i = 0; i < arr->length; i++) {
        chars[i] = (char) (arr->elements[i].number & 0xff);
    }
    chars[arr->length] = 0;
    fputs(chars, stdout);
    free(chars);
    return (itval) ((int64_t) 0);
}
itval rm_read(it_STACKFRAME* stackptr, itval* params) {
    it_ARRAY_DATA* buff = params[0].array_data;
    int64_t num_chars = params[1].number;
    if(num_chars > 1024) {
        num_chars = 1024;
    }
    // I could use a VLA here, but it wouldn't actualyl help anything
    char mybuff[1025];
    ssize_t result = read(STDIN_FILENO, mybuff, (size_t) num_chars);
    for(int i = 0; i < result; i++) {
        buff->elements[i].number = mybuff[i];
    }
    itval ret;
    ret.number = result;
    return ret;
}
itval rm_exit(it_STACKFRAME* stackptr, itval* params) {
    uint64_t exit_status = params[0].number;
    exit(exit_status);
    // This will never be reached
    return (itval) ((int64_t) 0);
}
itval rm_traceback(it_STACKFRAME* stackptr, itval* params) {
    it_traceback(stackptr);
    return (itval) ((int64_t) 0);
}
void it_create_replaced_method(it_PROGRAM* prog, int method_index, int nargs, it_METHOD_REPLACEMENT_PTR methodptr, char* name) {
    if(method_index >= prog->methodc) {
        fatal("Attempting to create replaced method with index greater than methodc. This is a bug.");
    }
    prog->methods[method_index].register_types = NULL;
    prog->methods[method_index].containing_clazz = NULL;
    prog->methods[method_index].id = prog->method_id++;
    prog->methods[method_index].name = name;
    prog->methods[method_index].nargs = nargs;
    prog->methods[method_index].opcodec = 0;
    prog->methods[method_index].opcodes = NULL;
    prog->methods[method_index].registerc = 0;
    prog->methods[method_index].returntype = NULL;
    prog->methods[method_index].replacement_ptr = methodptr;
}
void it_replace_methods(it_PROGRAM* prog) {
    int method_index = prog->methodc - NUM_REPLACED_METHODS;
    it_create_replaced_method(prog, method_index++, 1, rm_print, strdup("stdlib.internal.print"));
    it_create_replaced_method(prog, method_index++, 0, rm_exit, strdup("stdlib.internal.exit"));
    it_create_replaced_method(prog, method_index++, 1, rm_read, strdup("stdlib.internal.read"));
    it_create_replaced_method(prog, method_index++, 0, rm_traceback, strdup("stdlib.internal.traceback"));
}
