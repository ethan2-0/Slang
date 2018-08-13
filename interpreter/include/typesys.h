#include <stdint.h>

union ts_TYPE;
struct ts_TYPE_BAREBONES {
    uint32_t id;
    int heirarchy_len;
    // I use this next field as a variable-length array, even though it's
    // defined here as a scalar, since structs can't have variable-length arrays
    // (at least AFAICT). So instead I just malloc whatever extra room I need
    // and do pointer arithmetic.
    union ts_TYPE* heirarchy;
};
union ts_TYPE {
    struct ts_TYPE_BAREBONES barebones;
};
typedef union ts_TYPE ts_TYPE;
typedef struct ts_TYPE_BAREBONES ts_TYPE_BAREBONES;
ts_TYPE* get_type(char* name);
