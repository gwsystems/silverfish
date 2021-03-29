#define IMPORT __attribute__((visibility("default")))
#define EXPORT __attribute__((visibility("default")))

IMPORT void __init_libc(char**, char*);

void dummy() {
    __init_libc(0, 0);
}
