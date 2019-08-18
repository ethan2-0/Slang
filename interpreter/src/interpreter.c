#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "common.h"
#include "interpreter.h"
#include "opcodes.h"
#include "nativelibs.h"

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
    it_replace_methods(prog);
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
    ts_reify_generic_references(&prog->methods[prog->entrypoint]);
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
        case OPCODE_CLASSCALLSPECIAL:
            ; // Labels can't be immediately followed by declarations, because
              // C is a barbaric language
            // TODO: Especially after the introduction of CLASSCALLSPECIAL,
            // this desperately needs optimization.
            struct it_METHOD* callee;
            if(iptr->type == OPCODE_CALL) {
                callee = ((struct it_OPCODE_DATA_CALL*) iptr->payload)->callee;
            } else {
                callee = ((struct it_OPCODE_DATA_CLASSCALLSPECIAL*) iptr->payload)->method;
            }
            if(!callee->has_had_references_reified) {
                ts_reify_generic_references(callee);
            }
            if(iptr->type == OPCODE_CLASSCALLSPECIAL) {
                if(registers[((struct it_OPCODE_DATA_CLASSCALLSPECIAL*) iptr->payload)->callee_register].clazz_data == NULL) {
                    it_traceback(stackptr);
                    fatal("Attempt to do a non-virtual class call on null");
                }
            }
            if(callee->replacement_ptr != NULL) {
                #if DEBUG
                printf("Calling replaced method %s\n", callee->name);
                #endif
                stackptr->iptr = iptr;
                uint32_t returnreg = ((struct it_OPCODE_DATA_CALL*) iptr->payload)->returnval;
                registers[returnreg] = callee->replacement_ptr(prog, stackptr, params);
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
            if(callee->containing_clazz != NULL) {
                printf("Calling %s from %s\n", callee->name, callee->containing_clazz->name);
            } else {
                printf("Calling %s\n", callee->name);
            }
            #endif

            if(iptr->type == OPCODE_CLASSCALLSPECIAL) {
                oldstack->returnreg = ((struct it_OPCODE_DATA_CLASSCALLSPECIAL*) iptr->payload)->destination_register;
            } else {
                oldstack->returnreg = ((struct it_OPCODE_DATA_CALL*) iptr->payload)->returnval;
            }

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
            if(oldstack->iptr->type == OPCODE_CLASSCALLSPECIAL) {
                uint32_t reg = ((struct it_OPCODE_DATA_CLASSCALLSPECIAL*) oldstack->iptr->payload)->callee_register;
                registers[0] = oldstack->registers[reg];
            }
            for(uint32_t i = 0; i < callee->nargs - (oldstack->iptr->type == OPCODE_CLASSCALLSPECIAL ? 1 : 0); i++) {
                registers[i + (oldstack->iptr->type == OPCODE_CALL ? 0 : 1)] = params[i];
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

            struct it_OPCODE_DATA_CLASSCALL* classcalldata = ((struct it_OPCODE_DATA_CLASSCALL*) iptr->payload);

            uint32_t classcall_callee_index = classcalldata->callee_index;

            classcall_oldstack->returnreg = classcalldata->returnreg;
            union itval thiz = registers[classcalldata->targetreg];
            if(thiz.clazz_data == NULL) {
                stackptr--;
                it_traceback(stackptr);
                fatal("Attempt to call method of null");
            }
            struct it_METHOD* classcall_callee = thiz.clazz_data->method_table->methods[classcall_callee_index];

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
        case OPCODE_INTERFACECALL:
            {
                struct it_STACKFRAME* oldstack = stackptr;
                stackptr++;
                if(stackptr >= stackend) {
                    stackptr--;
                    it_traceback(stackptr);
                    fatal("Stack overflow");
                }
                struct it_OPCODE_DATA_INTERFACECALL* data = ((struct it_OPCODE_DATA_INTERFACECALL*) iptr->payload);
                oldstack->returnreg = data->destination_register;
                union itval thiz = registers[data->callee_register];
                if(thiz.clazz_data == NULL) {
                    stackptr--;
                    it_traceback(stackptr);
                    fatal("Attempt to call method of null interface reference");
                }
                struct it_INTERFACE_IMPLEMENTATION* implementations = thiz.clazz_data->method_table->interface_implementations;
                struct it_INTERFACE_IMPLEMENTATION* implementation;
                for(int i = 0; i < thiz.clazz_data->method_table->ninterface_implementations; i++) {
                    if(implementations[i].interface_id == data->interface_id) {
                        implementation = &implementations[i];
                        break;
                    }
                }
                struct it_METHOD* callee = implementation->method_table->methods[data->method_index];
                load_method(stackptr, callee);
                registers = stackptr->registers;
                instruction_start = stackptr->iptr;
                oldstack->iptr = iptr;
                iptr = instruction_start;
                registers[0] = thiz;
                for(uint32_t i = 0; i < callee->nargs - 1; i++) {
                    registers[i + 1] = params[i];
                }
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
            #if DEBUG
            printf("Returned to %s\n", stackptr->method->name);
            #endif
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
            registers[opcode_new_data->dest].clazz_data->method_table = opcode_new_data->clazz->method_table;
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
                union ts_TYPE* target_type = stackptr->method->register_types[data->target];
                union ts_TYPE* source_register_type = stackptr->method->register_types[data->source];
                if(source_register_type->barebones.category == ts_CATEGORY_PRIMITIVE) {
                    if(target_type->barebones.category != ts_CATEGORY_PRIMITIVE) {
                        fatal("Incompatible cast");
                    }
                } else if(source_register_type->barebones.category == ts_CATEGORY_ARRAY) {
                    if(!ts_is_compatible((union ts_TYPE*) source.array_data->type, target_type)) {
                        fatal("Incompatible cast");
                    }
                } else {
                    // We know data->source_register_type->barebones.category == ts_CATEGORY_CLAZZ or ts_CATEGORY_INTERFACE
                    if(!ts_is_compatible((union ts_TYPE*) &source.clazz_data->method_table->type->clazz, target_type)) {
                        it_traceback(stackptr);
                        fatal("Incompatible cast");
                    }
                }
                registers[data->target] = source;
                iptr++;
                continue;
            }
        case OPCODE_INSTANCEOF:
            {
                struct it_OPCODE_DATA_INSTANCEOF* data = (struct it_OPCODE_DATA_INSTANCEOF*) iptr->payload;
                union ts_TYPE* source_register_type = stackptr->method->register_types[data->source];
                union ts_TYPE* predicate_type = stackptr->method->typereferences[data->predicate_type_index];
                bool result;
                if(source_register_type->barebones.category == ts_CATEGORY_PRIMITIVE) {
                    result = ts_instanceof(source_register_type, predicate_type);
                } else if(source_register_type->barebones.category == ts_CATEGORY_TYPE_PARAMETER) {
                    fatal("Instanceof parameter wasn't reified. This is an interpreter bug.");
                } else if(source_register_type->barebones.category == ts_CATEGORY_CLAZZ) {
                    union itval source = registers[data->source];
                    union ts_TYPE* source_type = (union ts_TYPE*) &source.clazz_data->method_table->type->clazz;
                    result = ts_instanceof(source_type, predicate_type);
                } else if(source_register_type->barebones.category == ts_CATEGORY_ARRAY) {
                    union itval source = registers[data->source];
                    union ts_TYPE* source_type = (union ts_TYPE*) source.array_data->type;
                    result = ts_instanceof(source_type, predicate_type);
                }
                registers[data->destination] = (union itval) ((int64_t) result);
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
void it_replace_method_from_lib(struct it_PROGRAM* program, struct nl_NATIVE_LIB* lib, char* symbol_name, char* method_name) {
    it_METHOD_REPLACEMENT_PTR method = nl_get_symbol(lib, symbol_name);
    for(int i = 0; i < program->methodc; i++) {
        if(strcmp(program->methods[i].name, method_name) == 0) {
            program->methods[i].replacement_ptr = method;
            return;
        }
    }
    printf("Method: %s\n", method_name);
    fatal("Failed to find method to replace");
}
union itval rm_traceback(struct it_PROGRAM* program, struct it_STACKFRAME* stackptr, union itval* params) {
    it_traceback(stackptr);
    return (union itval) ((int64_t) 0);
}
union itval rm_replace(struct it_PROGRAM* program, struct it_STACKFRAME* stackptr, union itval* params) {
    uint64_t native_lib_id = params[0].number;
    struct it_ARRAY_DATA* symbol_name_array = params[1].array_data;
    char* symbol_name = mm_malloc(symbol_name_array->length + 1);
    for(int i = 0; i < symbol_name_array->length; i++) {
        // I mean, technically, symbol names are just byte arrays anyway
        symbol_name[i] = (char) symbol_name_array->elements[i].number;
    }
    struct it_ARRAY_DATA* method_name_array = params[2].array_data;
    char* method_name = mm_malloc(method_name_array->length + 1);
    for(int i = 0; i < method_name_array->length; i++) {
        method_name[i] = (char) method_name_array->elements[i].number;
    }
    struct nl_NATIVE_LIB* lib = nl_get_by_index(native_lib_id);
    it_replace_method_from_lib(program, lib, symbol_name, method_name);
    free(symbol_name);
    free(method_name);
    union itval result;
    result.number = 0;
    return result;
}
union itval rm_create_rtlib(struct it_PROGRAM* program, struct it_STACKFRAME* stackptr, union itval* params) {
    struct nl_NATIVE_LIB* rtlib = nl_create_rtlib();
    union itval result;
    result.number = rtlib->id;
    return result;
}
union itval rm_create_lib(struct it_PROGRAM* program, struct it_STACKFRAME* stackptr, union itval* params) {
    struct it_ARRAY_DATA* path_name_array = params[0].array_data;
    char* path_name = mm_malloc(path_name_array->length + 1);
    for(int i = 0; i < path_name_array->length; i++) {
        // I mean, technically, symbol names are just byte arrays anyway
        path_name[i] = (char) path_name_array->elements[i].number;
    }
    union itval result;
    result.number = nl_create_native_lib_handle(path_name)->id;
    return result;
}
union itval rm_close_native_lib(struct it_PROGRAM* program, struct it_STACKFRAME* stackptr, union itval* params) {
    uint64_t native_lib_id = params[0].number;
    struct nl_NATIVE_LIB* native_lib = nl_get_by_index(native_lib_id);
    nl_close(native_lib);
    union itval result;
    result.number = 0;
    return result;
}
void it_replace_methods(struct it_PROGRAM* program) {
    for(int i = 0; i < program->methodc; i++) {
        if(strcmp(program->methods[i].name, "stdlib.internal.traceback") == 0) {
            program->methods[i].replacement_ptr = rm_traceback;
        }
        if(strcmp(program->methods[i].name, "stdlib.internal.replace") == 0) {
            program->methods[i].replacement_ptr = rm_replace;
        }
        if(strcmp(program->methods[i].name, "stdlib.internal.create_rtlib") == 0) {
            program->methods[i].replacement_ptr = rm_create_rtlib;
        }
        if(strcmp(program->methods[i].name, "stdlib.internal.create_lib") == 0) {
            program->methods[i].replacement_ptr = rm_create_rtlib;
        }
        if(strcmp(program->methods[i].name, "stdlib.internal.close_native_lib") == 0) {
            program->methods[i].replacement_ptr = rm_close_native_lib;
        }
    }
}
