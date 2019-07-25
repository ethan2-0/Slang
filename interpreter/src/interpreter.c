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

void it_traceback(struct it_STACKFRAME* stackptr) {
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
void load_method(struct it_STACKFRAME* stackptr, struct it_METHOD* method) {
    stackptr->method = method;
    stackptr->iptr = stackptr->method->opcodes;
    stackptr->registerc = stackptr->method->registerc;
    if(stackptr->registers_allocated < stackptr->registerc) {
        #if DEBUG
        printf("Allocating more register space: %d < %d\n", stackptr->registers_allocated, stackptr->registerc);
        #endif
        free(stackptr->registers);
        stackptr->registers = mm_malloc(sizeof(union itval) * stackptr->registerc);
    }
    // Technically, we've already zeroed out the memory (in mm_malloc(...)).
    // But that's mostly to ease debugging, I don't want to rely on it.
    memset(stackptr->registers, 0x00, stackptr->registerc * sizeof(union itval));
}
void it_execute(struct it_PROGRAM* prog, struct it_OPTIONS* options) {
    // Setup interpreter state
    struct it_STACKFRAME* stack = mm_malloc(sizeof(struct it_STACKFRAME) * STACKSIZE);
    struct it_STACKFRAME* stackptr = stack;
    struct it_STACKFRAME* stackend = stack + STACKSIZE;
    for(int i = 0; i < STACKSIZE; i++) {
        stack[i].registers_allocated = 0;
        // Set the register file pointer to the null pointer so we can free() it.
        stack[i].registers = 0;
        stack[i].index = i;
    }
    load_method(stackptr, &prog->methods[prog->entrypoint]);
    union itval* registers = stackptr->registers;
    #if DEBUG
    printf("About to execute method %d\n", stackptr->method->id);
    #endif
    // This is the start of the instruction tape
    struct it_OPCODE* instruction_start = stackptr->iptr;
    // This is the current index into the instruction tape
    struct it_OPCODE* iptr = instruction_start;
    // See the documentation for PARAM and CALL for details.
    // Note that this is preserved globally across method calls.
    union itval params[256];
    memset(params, 0, sizeof(union itval) * 256);

    // TODO: Do something smarter for dispatch here
    while(1) {
        #if DEBUG
        for(int i = 0; i < stackptr->registerc; i++) {
            printf("%02x ", registers[i]);
        }
        printf("\n");
        printf("Type %02x, iptr %d\n", iptr->type, iptr - instruction_start);
        #endif
        switch(iptr->type) {
        case OPCODE_PARAM:
            params[((struct it_OPCODE_DATA_PARAM*) iptr->payload)->target] = registers[((struct it_OPCODE_DATA_PARAM*) iptr->payload)->source];
            iptr++;
            continue;
        case OPCODE_CALL:
            ; // Labels can't be immediately followed by declarations, because
              // C is a barbaric language
            uint32_t callee_id = ((struct it_OPCODE_DATA_CALL*) iptr->payload)->callee;
            struct it_METHOD* callee = &prog->methods[callee_id];
            if(callee->replacement_ptr != NULL) {
                #if DEBUG
                printf("Calling replaced method %s\n", callee->name);
                #endif
                stackptr->iptr = iptr;
                uint32_t returnreg = ((struct it_OPCODE_DATA_CALL*) iptr->payload)->returnval;
                registers[returnreg] = callee->replacement_ptr(stackptr, params);
                iptr++;
                continue;
            }
            struct it_STACKFRAME* oldstack = stackptr;
            stackptr++;
            if(stackptr >= stackend) {
                stackptr--;
                it_traceback(stackptr);
                fatal("Stack overflow");
            }

            #if DEBUG
            printf("Calling %d\n", callee_id);
            #endif

            oldstack->returnreg = ((struct it_OPCODE_DATA_CALL*) iptr->payload)->returnval;

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
            struct it_STACKFRAME* classcall_oldstack = stackptr;
            stackptr++;
            if(stackptr >= stackend) {
                stackptr--;
                it_traceback(stackptr);
                fatal("Stack overflow");
            }

            uint32_t classcall_callee_index = ((struct it_OPCODE_DATA_CLASSCALL*) iptr->payload)->callee_index;

            classcall_oldstack->returnreg = ((struct it_OPCODE_DATA_CLASSCALL*) iptr->payload)->returnreg;
            union itval thiz = registers[((struct it_OPCODE_DATA_CLASSCALL*) iptr->payload)->targetreg];
            struct it_METHOD* classcall_callee = thiz.clazz_data->phi_table->methods[classcall_callee_index];

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
            union itval result = registers[((struct it_OPCODE_DATA_RETURN*) iptr->payload)->target];
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
            registers[((struct it_OPCODE_DATA_ZERO*) iptr->payload)->target].number = 0x00000000;
            iptr++;
            continue;
        case OPCODE_ADD:
            registers[((struct it_OPCODE_DATA_ADD*) iptr->payload)->target].number = registers[((struct it_OPCODE_DATA_ADD*) iptr->payload)->source1].number + registers[((struct it_OPCODE_DATA_ADD*) iptr->payload)->source2].number;
            iptr++;
            continue;
        case OPCODE_TWOCOMP:
            registers[((struct it_OPCODE_DATA_TWOCOMP*) iptr->payload)->target].number = -registers[((struct it_OPCODE_DATA_TWOCOMP*) iptr->payload)->target].number;
            iptr++;
            continue;
        case OPCODE_MULT:
            registers[((struct it_OPCODE_DATA_MULT*) iptr->payload)->target].number = registers[((struct it_OPCODE_DATA_MULT*) iptr->payload)->source1].number * registers[((struct it_OPCODE_DATA_MULT*) iptr->payload)->source2].number;
            iptr++;
            continue;
        case OPCODE_MODULO:
            registers[((struct it_OPCODE_DATA_MODULO*) iptr->payload)->target].number = registers[((struct it_OPCODE_DATA_MODULO*) iptr->payload)->source1].number % registers[((struct it_OPCODE_DATA_MODULO*) iptr->payload)->source2].number;
            iptr++;
            continue;
        case OPCODE_DIV:
            registers[((struct it_OPCODE_DATA_MODULO*) iptr->payload)->target].number = registers[((struct it_OPCODE_DATA_MODULO*) iptr->payload)->source1].number / registers[((struct it_OPCODE_DATA_MODULO*) iptr->payload)->source2].number;
            iptr++;
            continue;
        case OPCODE_EQUALS:
            // TODO: Make this work on systems where sizeof(union itval.number) != sizeof(union itval.clazz_data), etc
            //       This probably involves a specialized EQUALS opcode, either in the compiler or
            //       just specializing in bytecode.c.
            if(registers[((struct it_OPCODE_DATA_EQUALS*) iptr->payload)->source1].number == registers[((struct it_OPCODE_DATA_EQUALS*) iptr->payload)->source2].number) {
                registers[((struct it_OPCODE_DATA_EQUALS*) iptr->payload)->target].number = 0x1;
            } else {
                registers[((struct it_OPCODE_DATA_EQUALS*) iptr->payload)->target].number = 0x0;
            }
            iptr++;
            continue;
        case OPCODE_INVERT:
            if(registers[((struct it_OPCODE_DATA_INVERT*) iptr->payload)->target].number == 0x0) {
                registers[((struct it_OPCODE_DATA_INVERT*) iptr->payload)->target].number = 0x1;
            } else {
                registers[((struct it_OPCODE_DATA_INVERT*) iptr->payload)->target].number = 0x0;
            }
            iptr++;
            continue;
        case OPCODE_LTEQ:
            if(registers[((struct it_OPCODE_DATA_LTEQ*) iptr->payload)->source1].number <= registers[((struct it_OPCODE_DATA_LTEQ*) iptr->payload)->source2].number) {
                registers[((struct it_OPCODE_DATA_LTEQ*) iptr->payload)->target].number = 0x1;
            } else {
                registers[((struct it_OPCODE_DATA_LTEQ*) iptr->payload)->target].number = 0x0;
            }
            iptr++;
            continue;
        case OPCODE_GT:
            if(registers[((struct it_OPCODE_DATA_GT*) iptr->payload)->source1].number > registers[((struct it_OPCODE_DATA_GT*) iptr->payload)->source2].number) {
                registers[((struct it_OPCODE_DATA_GT*) iptr->payload)->target].number = 0x1;
            } else {
                registers[((struct it_OPCODE_DATA_GT*) iptr->payload)->target].number = 0x0;
            }
            iptr++;
            continue;
        case OPCODE_GOTO:
            #if DEBUG
            printf("Jumping to %d\n", ((struct it_OPCODE_DATA_GOTO*) iptr->payload)->target);
            #endif
            iptr = instruction_start + ((struct it_OPCODE_DATA_GOTO*) iptr->payload)->target;
            continue;
        case OPCODE_JF:
            // JF stands for Jump if False.
            if(registers[((struct it_OPCODE_DATA_JF*) iptr->payload)->predicate].number == 0x00) {
                #if DEBUG
                printf("Jumping to %d\n", ((struct it_OPCODE_DATA_JF*) iptr->payload)->target);
                #endif
                iptr = instruction_start + ((struct it_OPCODE_DATA_JF*) iptr->payload)->target;
            } else {
                iptr++;
            }
            continue;
        case OPCODE_GTEQ:
            if(registers[((struct it_OPCODE_DATA_GTEQ*) iptr->payload)->source1].number >= registers[((struct it_OPCODE_DATA_GTEQ*) iptr->payload)->source2].number) {
                registers[((struct it_OPCODE_DATA_GTEQ*) iptr->payload)->target].number = 0x1;
            } else {
                registers[((struct it_OPCODE_DATA_GTEQ*) iptr->payload)->target].number = 0x0;
            }
            iptr++;
            continue;
        case OPCODE_XOR:
            registers[((struct it_OPCODE_DATA_XOR*) iptr->payload)->target].number = registers[((struct it_OPCODE_DATA_XOR*) iptr->payload)->source1].number ^ registers[((struct it_OPCODE_DATA_XOR*) iptr->payload)->source2].number;
            iptr++;
            continue;
        case OPCODE_AND:
            registers[((struct it_OPCODE_DATA_AND*) iptr->payload)->target].number = registers[((struct it_OPCODE_DATA_AND*) iptr->payload)->source1].number & registers[((struct it_OPCODE_DATA_AND*) iptr->payload)->source2].number;
            iptr++;
            continue;
        case OPCODE_OR:
            registers[((struct it_OPCODE_DATA_OR*) iptr->payload)->target].number = registers[((struct it_OPCODE_DATA_OR*) iptr->payload)->source1].number | registers[((struct it_OPCODE_DATA_OR*) iptr->payload)->source2].number;
            iptr++;
            continue;
        case OPCODE_MOV:
            registers[((struct it_OPCODE_DATA_MOV*) iptr->payload)->target] = registers[((struct it_OPCODE_DATA_MOV*) iptr->payload)->source];
            iptr++;
            continue;
        case OPCODE_NOP:
            // Do nothing
            iptr++;
            continue;
        case OPCODE_LOAD:
            registers[((struct it_OPCODE_DATA_LOAD*) iptr->payload)->target].number = ((struct it_OPCODE_DATA_LOAD*) iptr->payload)->data;
            iptr++;
            continue;
        case OPCODE_LT:
            if(registers[((struct it_OPCODE_DATA_LT*) iptr->payload)->source1].number < registers[((struct it_OPCODE_DATA_LT*) iptr->payload)->source2].number) {
                registers[((struct it_OPCODE_DATA_LT*) iptr->payload)->target].number = 0x1;
            } else {
                registers[((struct it_OPCODE_DATA_LT*) iptr->payload)->target].number = 0x0;
            }
            iptr++;
            continue;
        case OPCODE_NEW:
            ;
            if(gc_needs_collection) {
                gc_collect(prog, stack, stackptr, options);
            }
            struct it_OPCODE_DATA_NEW* opcode_new_data = (struct it_OPCODE_DATA_NEW*) iptr->payload;
            size_t new_allocation_size = sizeof(struct it_CLAZZ_DATA) + sizeof(union itval) * opcode_new_data->clazz->nfields;
            registers[opcode_new_data->dest].clazz_data = mm_malloc(new_allocation_size);
            struct gc_OBJECT_REGISTRY* new_register = gc_register_object(registers[opcode_new_data->dest], new_allocation_size, ts_CATEGORY_CLAZZ);
            registers[opcode_new_data->dest].clazz_data->gc_registry_entry = new_register;
            memset(registers[opcode_new_data->dest].clazz_data->itval, 0, sizeof(union itval) * opcode_new_data->clazz->nfields);
            registers[opcode_new_data->dest].clazz_data->phi_table = opcode_new_data->clazz;
            iptr++;
            continue;
        case OPCODE_ACCESS:
            ;
            struct it_OPCODE_DATA_ACCESS* opcode_access_data = (struct it_OPCODE_DATA_ACCESS*) iptr->payload;
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
            struct it_OPCODE_DATA_ASSIGN* opcode_assign_data = (struct it_OPCODE_DATA_ASSIGN*) iptr->payload;
            if(registers[opcode_assign_data->clazzreg].clazz_data == NULL) {
                it_traceback(stackptr);
                fatal("Null pointer on access");
            }
            registers[opcode_assign_data->clazzreg].clazz_data->itval[opcode_assign_data->property_index] = registers[opcode_assign_data->source];
            iptr++;
            continue;
        case OPCODE_ARRALLOC:
            ;
            if(gc_needs_collection) {
                gc_collect(prog, stack, stackptr, options);
            }
            struct it_OPCODE_DATA_ARRALLOC* opcode_arralloc_data = (struct it_OPCODE_DATA_ARRALLOC*) iptr->payload;
            uint64_t length = registers[opcode_arralloc_data->lengthreg].number;
            size_t arralloc_allocation_size = sizeof(struct it_ARRAY_DATA) + sizeof(union itval) * (length <= 0 ? 1 : length - 1);
            registers[opcode_arralloc_data->arrreg].array_data = (struct it_ARRAY_DATA*) mm_malloc(arralloc_allocation_size);
            registers[opcode_arralloc_data->arrreg].array_data->length = length;
            memset(&registers[opcode_arralloc_data->arrreg].array_data->elements, 0, length * sizeof(union itval));
            registers[opcode_arralloc_data->arrreg].array_data->type = (struct ts_TYPE_ARRAY*) stackptr->method->register_types[opcode_arralloc_data->arrreg];
            struct gc_OBJECT_REGISTRY* arralloc_registry = gc_register_object(registers[opcode_arralloc_data->arrreg], arralloc_allocation_size, ts_CATEGORY_ARRAY);
            registers[opcode_arralloc_data->arrreg].array_data->gc_registry_entry = arralloc_registry;
            iptr++;
            continue;
        case OPCODE_ARRACCESS:
            ;
            struct it_OPCODE_DATA_ARRACCESS* opcode_arraccess_data = (struct it_OPCODE_DATA_ARRACCESS*) iptr->payload;
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
            struct it_OPCODE_DATA_ARRASSIGN* opcode_arrassign_data = (struct it_OPCODE_DATA_ARRASSIGN*) iptr->payload;
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
            struct it_OPCODE_DATA_ARRLEN* opcode_arrlen_data = (struct it_OPCODE_DATA_ARRLEN*) iptr->payload;
            if(registers[opcode_arrlen_data->arrreg].array_data == NULL) {
                it_traceback(stackptr);
                fatal("Attempt to find length of null");
            }
            registers[opcode_arrlen_data->resultreg].number = registers[opcode_arrlen_data->arrreg].array_data->length;
            iptr++;
            continue;
        case OPCODE_CAST:
            {
                struct it_OPCODE_DATA_CAST* data = (struct it_OPCODE_DATA_CAST*) iptr->payload;
                union itval source = registers[data->source];
                // We know data->source_register_type->barebones.category == ts_CATEGORY_CLAZZ
                if(!ts_is_compatible((union ts_TYPE*) source.clazz_data->phi_table, data->target_type)) {
                    it_traceback(stackptr);
                    fatal("Incompatible cast");
                }
                registers[data->target] = source;
                iptr++;
                continue;
            }
        case OPCODE_INSTANCEOF:
            {
                struct it_OPCODE_DATA_INSTANCEOF* data = (struct it_OPCODE_DATA_INSTANCEOF*) iptr->payload;
                union itval source = registers[data->source];
                registers[data->destination] = (union itval) ((int64_t) ts_instanceof((union ts_TYPE*) source.clazz_data->phi_table, data->predicate_type));
                iptr++;
                continue;
            }
        case OPCODE_STATICVARGET:
            {
                struct it_OPCODE_DATA_STATICVARGET* data = (struct it_OPCODE_DATA_STATICVARGET*) iptr->payload;
                registers[data->destination] = prog->static_vars[data->source_var].value;
                iptr++;
                continue;
            }
        case OPCODE_STATICVARSET:
            {
                struct it_OPCODE_DATA_STATICVARSET* data = (struct it_OPCODE_DATA_STATICVARSET*) iptr->payload;
                prog->static_vars[data->destination_var].value = registers[data->source];
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
void it_run(struct it_PROGRAM* prog, struct it_OPTIONS* options) {
    #if DEBUG
    printf("Entering it_RUN\n");
    #endif
    it_execute(prog, options);
}
union itval rm_print(struct it_STACKFRAME* stackptr, union itval* params) {
    struct it_ARRAY_DATA* arr = params[0].array_data;
    char* chars = mm_malloc(sizeof(char) * (arr->length + 1));
    for(int i = 0; i < arr->length; i++) {
        chars[i] = (char) (arr->elements[i].number & 0xff);
    }
    chars[arr->length] = 0;
    fputs(chars, stdout);
    free(chars);
    return (union itval) ((int64_t) 0);
}
union itval rm_read(struct it_STACKFRAME* stackptr, union itval* params) {
    struct it_ARRAY_DATA* buff = params[0].array_data;
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
    union itval ret;
    ret.number = result;
    return ret;
}
union itval rm_exit(struct it_STACKFRAME* stackptr, union itval* params) {
    uint64_t exit_status = params[0].number;
    exit(exit_status);
    // This will never be reached
    return (union itval) ((int64_t) 0);
}
union itval rm_traceback(struct it_STACKFRAME* stackptr, union itval* params) {
    it_traceback(stackptr);
    return (union itval) ((int64_t) 0);
}
void it_create_replaced_method(struct it_PROGRAM* prog, int method_index, int nargs, it_METHOD_REPLACEMENT_PTR methodptr, char* name) {
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
void it_replace_methods(struct it_PROGRAM* prog) {
    int method_index = prog->methodc - NUM_REPLACED_METHODS;
    it_create_replaced_method(prog, method_index++, 1, rm_print, strdup("stdlib.internal.print"));
    it_create_replaced_method(prog, method_index++, 0, rm_exit, strdup("stdlib.internal.exit"));
    it_create_replaced_method(prog, method_index++, 1, rm_read, strdup("stdlib.internal.read"));
    it_create_replaced_method(prog, method_index++, 0, rm_traceback, strdup("stdlib.internal.traceback"));
}
