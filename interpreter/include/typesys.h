#include <stdint.h>

#ifndef TYPESYS_H
#define TYPESYS_H

union ts_TYPE;

enum ts_CATEGORY {
    ts_CATEGORY_PRIMITIVE,
    ts_CATEGORY_CLAZZ
};
struct ts_TYPE_BAREBONES {
    enum ts_CATEGORY category;
    uint32_t id;
    int heirarchy_len;
    // This is a pointer to an array of pointers
    union ts_TYPE** heirarchy;
};
struct ts_CLAZZ_FIELD {
    char* name;
    union ts_TYPE* type;
};
struct ts_TYPE_CLAZZ {
    enum ts_CATEGORY category;
    uint32_t id;
    int heirarchy_len;
    // This is a pointer to an array of pointers
    union ts_TYPE** heirarchy;
    int nfields;
    struct ts_CLAZZ_FIELD* fields;
};
union ts_TYPE {
    struct ts_TYPE_BAREBONES barebones;
    struct ts_TYPE_CLAZZ clazz;
};

typedef union ts_TYPE ts_TYPE;
typedef struct ts_TYPE_BAREBONES ts_TYPE_BAREBONES;
typedef struct ts_TYPE_CLAZZ ts_TYPE_CLAZZ;
typedef struct ts_CLAZZ_FIELD ts_CLAZZ_FIELD;
typedef enum ts_CATEGORY ts_CATEGORY;

ts_TYPE* ts_get_type(char* name);
int ts_get_field_index(ts_TYPE_CLAZZ* clazz, char* name);
void ts_register_type(ts_TYPE* type, char* name);
uint32_t ts_allocate_type_id();

#endif
