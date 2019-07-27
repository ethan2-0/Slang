#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include "common.h"
#include "nativelibs.h"
#include "whereami.h"

#define nl_NATIVE_LIB_REGISTRY_STARTING_SIZE 8

// This is a pointer to the start of an array
static struct nl_NATIVE_LIB* nl_native_libs = NULL;
static int nl_native_lib_size = 0;
static int nl_native_lib_index = 0;

struct nl_NATIVE_LIB* nl_register_new_native_lib() {
    if(nl_native_libs == NULL) {
        // Initialize the native library registry
        nl_native_lib_size = nl_NATIVE_LIB_REGISTRY_STARTING_SIZE;
        nl_native_libs = mm_malloc(sizeof(struct nl_NATIVE_LIB) * nl_native_lib_size);
    }
    if(nl_native_lib_index >= nl_native_lib_size) {
        int new_native_lib_size = nl_native_lib_size * 2;
        if(new_native_lib_size <= nl_native_lib_size) {
            // This is probably unnecessary but can't hurt
            fatal("new_native_lib_size <= nl_native_lib_size");
        }
        struct nl_NATIVE_LIB* new_native_libs = mm_malloc(sizeof(struct nl_NATIVE_LIB) * new_native_lib_size);
        memcpy(new_native_libs, nl_native_libs, sizeof(struct nl_NATIVE_LIB) * nl_native_lib_size);
        free(nl_native_libs);
        nl_native_libs = new_native_libs;
        nl_native_lib_size = new_native_lib_size;
    }
    return &nl_native_libs[nl_native_lib_index++];
}
struct nl_NATIVE_LIB* nl_create_native_lib_handle(char* path) {
    static uint64_t lib_handle_id = 0;

    struct nl_NATIVE_LIB* lib_handle = nl_register_new_native_lib();
    lib_handle->path = strdup(path);
    lib_handle->lifecycle = nl_LIFECYCLE_OPEN;
    lib_handle->id = lib_handle_id++;
    lib_handle->dlhandle = dlopen(path, RTLD_NOW | RTLD_LOCAL | RTLD_NODELETE);
    char* error = dlerror();
    if(error != NULL) {
        // TODO: Better error handling here
        printf("%s\n", error);
        fatal("dlopen(...) error");
    }
    return lib_handle;
}
struct nl_NATIVE_LIB* nl_get_by_index(uint64_t id) {
    // I could have index just be an index directly into the array, but
    // that makes memory safety harder to guarantee, which is important
    // since this is exposed (indirectly) via a replaced method.
    for(int i = 0; i < nl_native_lib_index; i++) {
        if(nl_native_libs[i].id == id) {
            return &nl_native_libs[i];
        }
    }
    printf("Index: %ld\n", id);
    fatal("Attempt to find native library by unknown index");
    return NULL; // Unreachable
}
void* nl_get_symbol(struct nl_NATIVE_LIB* lib, char* symbol) {
    if(lib->lifecycle != nl_LIFECYCLE_OPEN) {
        fatal("Attempt to use closed native lib");
    }
    void* ret = dlsym(lib->dlhandle, symbol);
    char* error = dlerror();
    if(error != NULL || ret == NULL) {
        if(error != NULL) {
            printf("%s\n", error);
        }
        printf("Symbol: %s\n", symbol);
        fatal("dlsym(...) error");
    }
    return ret;
}
void nl_close(struct nl_NATIVE_LIB* lib) {
    if(lib->lifecycle != nl_LIFECYCLE_OPEN) {
        fatal("Attempt to close closed native lib");
    }
    dlclose(lib->dlhandle);
    char* error = dlerror();
    if(error != NULL) {
        printf("%s\n", error);
        fatal("dlclose(...) error");
    }
}
struct nl_NATIVE_LIB* nl_create_rtlib() {
    // TODO: move this logic into Slang code
    int length = wai_getExecutablePath(NULL, 0, NULL);
    char* path = mm_malloc(length + 1);
    int dirname_length;
    wai_getExecutablePath(path, length, &dirname_length);
    // TODO: Make this configurable, perhaps with a command-line switch,
    // or a #define
    char* suffix = "/rtlib.so";
    int result_path_length = dirname_length + strlen(suffix);
    char* result_path = mm_malloc(result_path_length + 1);
    memcpy(result_path, path, dirname_length);
    memcpy(result_path + dirname_length, suffix, strlen(suffix));
    result_path[result_path_length] = '\0';
    free(path);
    struct nl_NATIVE_LIB* ret = nl_create_native_lib_handle(result_path);
    free(result_path);
    return ret;
}
