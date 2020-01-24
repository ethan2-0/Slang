#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/time.h>
#include "interpreter.h"
#include "common.h"

// TODO: This entire file would benefit from a lot of optimization.
//       My goal here is to get something functional and understandable,
//       with performance a distant second.

static struct gc_OBJECT_REGISTRY* gc_first_registry = NULL;
static struct gc_OBJECT_REGISTRY* gc_last_registry = NULL;
static struct gc_OBJECT_REGISTRY* gc_current_empty = NULL;
static int64_t gc_registry_size = 64;

void gc_grow_registry() {
    #if DEBUG
    printf("Growing registry to %d\n", gc_registry_size);
    #endif
    uint64_t previous_object_id = gc_last_registry == NULL ? 0 : gc_last_registry->object_id + 1;
    struct gc_OBJECT_REGISTRY* new_segment = mm_malloc(sizeof(struct gc_OBJECT_REGISTRY) * gc_registry_size);
    for(int i = 0; i < gc_registry_size; i++) {
        if(i > 0) {
            new_segment[i].previous = &(new_segment[i - 1]);
        }
        if(i < gc_registry_size - 1) {
            new_segment[i].next = &(new_segment[i + 1]);
        }
        new_segment[i].is_present = false;
        new_segment[i].object.array_data = NULL;
        new_segment[i].object_id = previous_object_id++;
    }
    new_segment[0].previous = gc_last_registry;
    new_segment[gc_registry_size - 1].next = NULL;
    if(gc_first_registry == NULL) {
        gc_first_registry = new_segment;
    }
    if(gc_last_registry != NULL) {
        gc_last_registry->next = new_segment;
    }
    gc_last_registry = &new_segment[gc_registry_size - 1];
    gc_registry_size *= 2;
}
void gc_find_new_empty() {
    while(gc_current_empty->is_present) {
        if(gc_current_empty->next == NULL) {
            gc_grow_registry();
        }
        gc_current_empty = gc_current_empty->next;
    }
}
static size_t gc_heap_size = 0;
static size_t gc_next_collection_size = (1 << 24);
extern bool gc_needs_collection;
struct gc_OBJECT_REGISTRY* gc_register_object(union itval object, size_t allocation_size, enum ts_CATEGORY category) {
    if(gc_current_empty == NULL) {
        gc_grow_registry();
        gc_current_empty = gc_first_registry;
    }
    struct gc_OBJECT_REGISTRY* ret = gc_current_empty;
    ret->object = object;
    ret->is_present = true;
    ret->allocation_size = allocation_size;
    ret->category = category;
    ret->visited = 0;
    gc_find_new_empty();
    gc_heap_size += allocation_size;
    if(gc_heap_size > gc_next_collection_size) {
        gc_needs_collection = true;
    }
    return ret;
}
void gc_free_internal(struct gc_OBJECT_REGISTRY* registry_entry) {
    gc_heap_size -= registry_entry->allocation_size;
    // We're saying `.array_data` here, because the type information is lost in
    // the implicit conversion to void* anyway.
    // Memset to notice use-after-free
    memset(registry_entry->object.array_data, 0x3e, registry_entry->allocation_size);
    free(registry_entry->object.array_data);
    registry_entry->is_present = false;
}
void gc_free(struct gc_OBJECT_REGISTRY* registry_entry) {
    gc_free_internal(registry_entry);
}
struct gc_QUEUE_ENTRY {
    struct gc_OBJECT_REGISTRY* entry;
    struct gc_QUEUE_ENTRY* next;
};
struct gc_QUEUE_ENTRY_PROPERTIES {
    struct gc_QUEUE_ENTRY* start;
    struct gc_QUEUE_ENTRY* end;
};
void gc_add_to_queue(struct gc_OBJECT_REGISTRY* registry, struct gc_QUEUE_ENTRY_PROPERTIES* properties) {
    struct gc_QUEUE_ENTRY* entry = mm_malloc(sizeof(struct gc_QUEUE_ENTRY));
    entry->entry = registry;
    entry->next = NULL;
    if(properties->start == NULL) {
        properties->start = entry;
        properties->end = entry;
    } else {
        properties->end->next = entry;
        properties->end = entry;
    }
}
struct gc_OBJECT_REGISTRY* gc_pop_from_queue(struct gc_QUEUE_ENTRY_PROPERTIES* properties) {
    struct gc_QUEUE_ENTRY* queue_entry = properties->start;
    struct gc_OBJECT_REGISTRY* ret = properties->start->entry;
    properties->start = queue_entry->next;
    free(queue_entry);
    return ret;
}
static int gc_current_pass = 0;
void gc_collect(struct it_PROGRAM* program, struct it_STACKFRAME* stack, struct it_STACKFRAME* current_frame, struct it_OPTIONS* options) {
    size_t starting_heap_size = gc_heap_size;
    if(options->gc_verbose) {
        printf("Garbage collecting, starting heap size is %ld\n", gc_heap_size);
    }
    gc_current_pass++;
    struct gc_QUEUE_ENTRY_PROPERTIES properties;
    properties.start = NULL;
    properties.end = NULL;
    // This exists to catch bugs. It was initially debugging code but
    // it's cheap enough to leave in just in case.
    int add_parity = 0;
    for(struct it_STACKFRAME* currentptr = stack; currentptr <= current_frame; currentptr++) {
        struct it_METHOD* method = currentptr->method;
        for(int i = 0; i < method->registerc; i++) {
            if(currentptr->registers[i].array_data == NULL) {
                continue;
            }
            enum ts_CATEGORY category = method->register_types[i]->category;
            if(category == ts_CATEGORY_ARRAY) {
                add_parity++;
                struct it_ARRAY_DATA* array_data = currentptr->registers[i].array_data;
                gc_add_to_queue(array_data->gc_registry_entry, &properties);
            } else if(category == ts_CATEGORY_CLAZZ) {
                add_parity++;
                gc_add_to_queue(currentptr->registers[i].clazz_data->gc_registry_entry, &properties);
            }
        }
    }
    for(int i = 0; i < program->static_varsc; i++) {
        enum ts_CATEGORY category = program->static_vars[i].type->category;
        // Note that itval.array_data == NULL iff itval.clazz_data == NULL
        if(program->static_vars[i].value.array_data == NULL) {
            continue;
        }
        if(category == ts_CATEGORY_ARRAY) {
            add_parity++;
            struct it_ARRAY_DATA* array_data = program->static_vars[i].value.array_data;
            gc_add_to_queue(array_data->gc_registry_entry, &properties);
        } else if(category == ts_CATEGORY_CLAZZ) {
            add_parity++;
            struct it_CLAZZ_DATA* clazz_data = program->static_vars[i].value.clazz_data;
            gc_add_to_queue(clazz_data->gc_registry_entry, &properties);
        } else {
            // Do nothing
        }
    }
    while(properties.start != NULL) {
        struct gc_OBJECT_REGISTRY* entry = gc_pop_from_queue(&properties);
        add_parity--;
        if(entry->visited >= gc_current_pass) {
            continue;
        }
        entry->visited = gc_current_pass;
        if(entry->category == ts_CATEGORY_ARRAY) {
            struct it_ARRAY_DATA* arr_data = entry->object.array_data;
            if(arr_data->type->data.array.parent_type->category == ts_CATEGORY_PRIMITIVE) {
                continue;
            } else if(arr_data->type->data.array.parent_type->category == ts_CATEGORY_ARRAY) {
                for(int i = 0; i < arr_data->length; i++) {
                    if(arr_data->elements[i].array_data == NULL) {
                        continue;
                    }
                    add_parity++;
                    gc_add_to_queue(arr_data->elements[i].array_data->gc_registry_entry, &properties);
                }
            } else if(arr_data->type->data.array.parent_type->category == ts_CATEGORY_CLAZZ) {
                for(int i = 0; i < arr_data->length; i++) {
                    if(arr_data->elements[i].clazz_data == NULL) {
                        continue;
                    }
                    add_parity++;
                    gc_add_to_queue(arr_data->elements[i].clazz_data->gc_registry_entry, &properties);
                }
            }
        } else if(entry->category == ts_CATEGORY_CLAZZ) {
            #if ASSERTIONS
            if(ts_clazz_is_raw(entry->object.clazz_data->method_table->type)) {
                fatal("Garbage collector got a raw type");
            }
            #endif
            struct ts_TYPE* clazz_properties = entry->object.clazz_data->method_table->type;
            for(int i = 0; i < clazz_properties->data.clazz.nfields; i++) {
                if(entry->object.clazz_data->itval[i].array_data == NULL) {
                    continue;
                } else if(clazz_properties->data.clazz.fields[i].type->category == ts_CATEGORY_PRIMITIVE) {
                    continue;
                } else if(clazz_properties->data.clazz.fields[i].type->category == ts_CATEGORY_ARRAY) {
                    add_parity++;
                    gc_add_to_queue(entry->object.clazz_data->itval[i].array_data->gc_registry_entry, &properties);
                } else if(clazz_properties->data.clazz.fields[i].type->category == ts_CATEGORY_CLAZZ) {
                    add_parity++;
                    gc_add_to_queue(entry->object.clazz_data->itval[i].clazz_data->gc_registry_entry, &properties);
                }
            }
        } else {
            fatal("Somehow got an entry that isn't a class or array");
        }
    }
    if(add_parity != 0) {
        printf("Add parity failed: %d\n", add_parity);
        fatal("Add parity failed");
    }
    for(struct gc_OBJECT_REGISTRY* registry = gc_first_registry; registry != NULL; registry = registry->next) {
        if(!registry->is_present) {
            continue;
        }
        if(registry->visited < gc_current_pass) {
            gc_free(registry);
        }
    }
    gc_needs_collection = false;
    gc_next_collection_size = gc_heap_size * 2;
    if(gc_next_collection_size < (1 << 24)) {
        gc_next_collection_size = 1 << 24;
    }
    if(options->gc_verbose) {
        printf("Finished collection, heap size is %ld (freed %ld bytes or %4f%%)\n",
                gc_heap_size,
                starting_heap_size - gc_heap_size,
                (double) 100 * ((double) 1 - (double) gc_heap_size / (double) starting_heap_size));
    }
}
