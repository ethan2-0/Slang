#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "interpreter_export.h"

struct ts_TYPE* ts_get_type_optional(char* name, struct ts_GENERIC_TYPE_CONTEXT* context);
struct ts_TYPE* ts_get_type(char* name, struct ts_GENERIC_TYPE_CONTEXT* context);
void ts_register_type(struct ts_TYPE* type, char* name);
uint32_t ts_allocate_type_id();
bool ts_is_compatible(struct ts_TYPE* type1, struct ts_TYPE* type2);
bool ts_instanceof(struct ts_TYPE* lhs, struct ts_TYPE* rhs);
struct ts_GENERIC_TYPE_CONTEXT* ts_create_generic_type_context(uint32_t num_arguments, struct ts_GENERIC_TYPE_CONTEXT* parent);
struct ts_TYPE* ts_allocate_type_parameter(char* name, struct ts_GENERIC_TYPE_CONTEXT* context, struct ts_TYPE* extends, int implementsc, struct ts_TYPE** implements);
void ts_walk_and_reify_methods(struct it_PROGRAM* program);
void ts_reify_generic_references(struct it_METHOD* method);
bool ts_class_is_specialization(struct ts_TYPE* type);
bool ts_class_is_generic(struct ts_TYPE* type);
bool ts_clazz_is_raw(struct ts_TYPE* type);
bool ts_type_arguments_are_equivalent(struct ts_TYPE_ARGUMENTS* first, struct ts_TYPE_ARGUMENTS* second);
struct ts_TYPE_ARGUMENTS* ts_compose_type_arguments(struct ts_TYPE_ARGUMENTS* first, struct ts_TYPE_ARGUMENTS* second);
struct it_METHOD* ts_get_method_reification(struct it_METHOD* generic_method, struct ts_TYPE_ARGUMENTS* arguments);
struct ts_TYPE* ts_reify_type(struct ts_TYPE* type, struct ts_TYPE_ARGUMENTS* type_arguments);
struct ts_TYPE_ARGUMENTS* ts_create_type_arguments(struct ts_GENERIC_TYPE_CONTEXT* context, int typeargsc, struct ts_TYPE** typeargs);
void ts_update_method_reification(struct it_METHOD* method);
struct ts_GENERIC_TYPE_CONTEXT* ts_get_method_type_context(struct it_METHOD* method);
bool ts_method_is_generic(struct it_METHOD* method);

void sv_add_static_var(struct it_PROGRAM* program, struct ts_TYPE* type, char* name, union itval value);
struct sv_STATIC_VAR* sv_get_var_by_name(struct it_PROGRAM* program, char* name);
bool sv_has_var(struct it_PROGRAM* program, char* name);
uint32_t sv_get_static_var_index_by_name(struct it_PROGRAM* program, char* name);

struct it_OPTIONS {
    bool print_return_value;
    bool gc_verbose;
    bool no_gc;
};

void it_run(struct it_PROGRAM* prog, struct it_OPTIONS* options);
void it_execute(struct it_PROGRAM* prog, struct it_OPTIONS* options);
void it_replace_methods(struct it_PROGRAM* prog);

void cl_arrange_method_tables(struct it_PROGRAM* program);
int cl_get_field_index(struct ts_TYPE* clazz, char* name);
int cl_get_method_index_optional(struct it_METHOD_TABLE* method_table, char* name);
int cl_get_method_index(struct it_METHOD_TABLE* clazz, char* name);
struct ts_TYPE* cl_get_or_create_interface(char* name, struct it_PROGRAM* program);
bool cl_class_implements_interface(struct ts_TYPE* clazz, struct ts_TYPE* interface);
struct ts_TYPE* cl_specialize_class(struct ts_TYPE* class, struct ts_TYPE_ARGUMENTS* type_arguments);
void cl_update_class_specializations(struct it_PROGRAM* program);

struct gc_OBJECT_REGISTRY* gc_register_object(union itval object, size_t allocation_size, enum ts_CATEGORY category);
void gc_collect(struct it_PROGRAM* program, struct it_STACKFRAME* stack, struct it_STACKFRAME* current_frame, struct it_OPTIONS* options);
bool gc_needs_collection;

#endif /* INTERPRETER_H */
