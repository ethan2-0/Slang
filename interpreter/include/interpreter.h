typedef struct {
    uint8_t type;
    void* payload;
} it_OPCODE;
typedef struct {
    uint32_t id;
    // This could technically be a uint8_t, but ehh
    int nargs;
    int opcodec;
    char* name;
    uint32_t registerc;
    it_OPCODE* opcodes;
} it_METHOD;
typedef struct {
    // This is the number of methods
    int methodc;
    // This is the methods, in an arbitrary order
    it_METHOD* methods;
    // This is the current method ID
    uint32_t method_id;
    // This is the entrypoint, as an index into `methods`.
    int entrypoint;
} it_PROGRAM;

void it_RUN(it_PROGRAM* prog);
