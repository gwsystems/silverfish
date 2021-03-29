#include <stdio.h>
#include <stdint.h>

#define EXPORT __attribute__((visibility("default")))
#define IMPORT __attribute__((visibility("default")))

IMPORT int imported_function(int arg);

EXPORT int exported_function(int arg, uint64_t arg2, int arg3) {
    return arg + arg2 + arg3;
}

void _start() {
    printf("Hello %s", "World!");
}
