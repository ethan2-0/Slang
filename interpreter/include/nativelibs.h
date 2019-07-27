#include <stdint.h>

#ifndef NATIVELIBS_H
#define NATIVELIBS_H

enum nl_LIFECYCLE {
    nl_LIFECYCLE_OPEN,
    nl_LIFECYCLE_CLOSED
};

struct nl_NATIVE_LIB {
    void* dlhandle;
    char* path;
    enum nl_LIFECYCLE lifecycle;
    uint64_t id;
};

struct nl_NATIVE_LIB* nl_create_rtlib();
void nl_close(struct nl_NATIVE_LIB* lib);
void* nl_get_symbol(struct nl_NATIVE_LIB* lib, char* symbol);
struct nl_NATIVE_LIB* nl_get_by_index(uint64_t id);
struct nl_NATIVE_LIB* nl_create_native_lib_handle(char* path);

#endif /* NATIVELIBS_H */
