#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/time.h>
#include "interpreter.h"
#include "common.h"

// TODO: This entire file would benefit from a lot of optimization.
//       My goal here is to get something functional and understandable,
//       with performance a distant second.

static gc_OBJECT_REGISTRY* gc_first_registry = NULL;
static gc_OBJECT_REGISTRY* gc_last_registry = NULL;
static gc_OBJECT_REGISTRY* gc_current_empty = NULL;
static size_t gc_registry_size = 64;

void gc_grow_registry() {
    #if DEBUG
    printf("Growing registry to %d\n", gc_registry_size);
    #endif
    uint64_t previous_object_id = gc_last_registry == NULL ? 0 : gc_last_registry->object_id + 1;
    gc_OBJECT_REGISTRY* new_segment = mm_malloc(sizeof(gc_OBJECT_REGISTRY) * gc_registry_size);
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
gc_OBJECT_REGISTRY* gc_register_object(itval object, size_t allocation_size, ts_CATEGORY category) {
    if(gc_current_empty == NULL) {
        gc_grow_registry();
        gc_current_empty = gc_first_registry;
    }
    gc_OBJECT_REGISTRY* ret = gc_current_empty;
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
void gc_free_internal(gc_OBJECT_REGISTRY* registry_entry) {
    gc_heap_size -= registry_entry->allocation_size;
    // We're saying `.array_data` here, because the type information is lost in
    // the implicit conversion to void*.
    // Memset to notice use-after-free
    memset(registry_entry->object.array_data, 0x3e, registry_entry->allocation_size);
    free(registry_entry->object.array_data);
    registry_entry->is_present = false;
}
void gc_free(gc_OBJECT_REGISTRY* registry_entry) {
    gc_free_internal(registry_entry);
}
typedef struct {
    gc_OBJECT_REGISTRY* entry;
    struct gc_QUEUE_ENTRY* next;
} gc_QUEUE_ENTRY;
typedef struct {
    gc_QUEUE_ENTRY* start;
    gc_QUEUE_ENTRY* end;
} gc_QUEUE_ENTRY_PROPERTIES;
void gc_add_to_queue(gc_OBJECT_REGISTRY* registry, gc_QUEUE_ENTRY_PROPERTIES* properties) {
    gc_QUEUE_ENTRY* entry = mm_malloc(sizeof(gc_QUEUE_ENTRY));
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
gc_OBJECT_REGISTRY* gc_pop_from_queue(gc_QUEUE_ENTRY_PROPERTIES* properties) {
    gc_QUEUE_ENTRY* queue_entry = properties->start;
    gc_OBJECT_REGISTRY* ret = properties->start->entry;
    properties->start = queue_entry->next;
    free(queue_entry);
    return ret;
}
static int gc_current_pass = 0;
void gc_collect(it_STACKFRAME* stack, it_STACKFRAME* current_frame) {
    gc_current_pass++;
    gc_QUEUE_ENTRY_PROPERTIES properties;
    properties.start = NULL;
    properties.end = NULL;
    int added_to_queue = 0;
    for(it_STACKFRAME* currentptr = stack; currentptr <= current_frame; currentptr++) {
        it_METHOD* method = currentptr->method;
        for(int i = 0; i < method->registerc; i++) {
            if(currentptr->registers[i].array_data == NULL) {
                continue;
            }
            ts_CATEGORY category = method->register_types[i]->barebones.category;
            if(category == ts_CATEGORY_ARRAY) {
                added_to_queue++;
                it_ARRAY_DATA* array_data = currentptr->registers[i].array_data;
                gc_add_to_queue(array_data->gc_registry_entry, &properties);
            } else if(category == ts_CATEGORY_CLAZZ) {
                added_to_queue++;
                gc_add_to_queue(currentptr->registers[i].clazz_data->gc_registry_entry, &properties);
            }
        }
    }
    gc_OBJECT_REGISTRY* updated;
    while(properties.start != NULL) {
        gc_OBJECT_REGISTRY* entry = gc_pop_from_queue(&properties);
        entry->visited = gc_current_pass;
        if(entry->category == ts_CATEGORY_ARRAY) {
            it_ARRAY_DATA* arr_data = entry->object.array_data;
            updated = arr_data;
            if(arr_data->type->parent_type->barebones.category == ts_CATEGORY_PRIMITIVE) {
                continue;
            } else if(arr_data->type->parent_type->barebones.category == ts_CATEGORY_ARRAY) {
                for(int i = 0; i < arr_data->length; i++) {
                    if(arr_data->elements[i].array_data == NULL) {
                        continue;
                    }
                    gc_add_to_queue(arr_data->elements[i].array_data->gc_registry_entry, &properties);
                }
            } else if(arr_data->type->parent_type->barebones.category == ts_CATEGORY_CLAZZ) {
                for(int i = 0; i < arr_data->length; i++) {
                    if(arr_data->elements[i].clazz_data == NULL) {
                        continue;
                    }
                    gc_add_to_queue(arr_data->elements[i].clazz_data->gc_registry_entry, &properties);
                }
            }
        } else if(entry->category == ts_CATEGORY_CLAZZ) {
            ts_TYPE_CLAZZ* properties = entry->object.clazz_data->phi_table;
            for(int i = 0; i < properties->nfields; i++) {
                if(entry->object.clazz_data->itval[i].array_data == NULL) {
                    continue;
                } else if(properties->fields[i].type->barebones.category == ts_CATEGORY_PRIMITIVE) {
                    continue;
                } else if(properties->fields[i].type->barebones.category == ts_CATEGORY_ARRAY) {
                    gc_add_to_queue(entry->object.clazz_data->itval[i].array_data->gc_registry_entry, &properties);
                } else if(properties->fields[i].type->barebones.category == ts_CATEGORY_CLAZZ) {
                    gc_add_to_queue(entry->object.clazz_data->itval[i].clazz_data->gc_registry_entry, &properties);
                }
            }
        } else {
            fatal("Somehow got an entry that isn't a class or array");
        }
    }
    int to_free = 0;
    int to_remain = 0;
    for(gc_OBJECT_REGISTRY* registry = gc_first_registry; registry != NULL; registry = registry->next) {
        if(!registry->is_present) {
            continue;
        }
        if(registry->visited < gc_current_pass) {
            to_free++;
        } else {
            to_remain++;
        }
    }
    gc_needs_collection = false;
    gc_next_collection_size = gc_heap_size * 2;
    if(gc_next_collection_size < (1 << 24)) {
        gc_next_collection_size = 1 << 24;
    }
}
