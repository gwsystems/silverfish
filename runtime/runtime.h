#define EXPORT __attribute__ ((visibility ("default")))
#define IMPORT __attribute__ ((visibility ("default")))

#define INLINE __attribute__((always_inline))
#define WEAK __attribute__((weak))

#if __has_include("assert.h") && (defined(__APPLE__) || defined(__linux__))
#include <assert.h>
#define awsm_assert assert
#else
int printf_(const char* format, ...);
#define awsm_assert(x) do { if(!(x)) { char msg[] = "" #x ""; printf_("%s\n", msg); while(1); } } while(0);
#endif

// Type alias's so I don't have to write uint32_t a million times
typedef signed char i8;
typedef unsigned char u8;

#if __has_include("stdint.h")
#include <stdint.h>
typedef int16_t i16;
typedef uint16_t u16;
typedef int32_t i32;
typedef uint32_t u32;
typedef int64_t i64;
typedef uint64_t u64;
#else
// FIXME: Cortex-m specific hack
typedef signed short i16;
typedef unsigned short u16;
typedef signed int i32;
typedef unsigned int u32;
typedef signed long long i64;
typedef unsigned long long u64;
#endif

#if __has_include("string.h") && __has_include("math.h") && __has_include("stdio.h") && __has_include("stdlib.h")
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#else

#define size_t u32

#define CORTEX_M

void* memcpy(void *dest, const void *src, size_t len);
void *memset(void *s, int c, size_t n);
char* strcpy(char* dest, const char* src);
size_t strlen(const char* str);

double trunc(double x);
float truncf(float x);

#endif

#if __has_include("limits.h")
#include <limits.h>
#else
#define CHAR_BIT 8

#define INT8_MIN   (-1-0x7f)
#define INT16_MIN  (-1-0x7fff)
#define INT32_MIN  (-1-0x7fffffff)
#define INT64_MIN  (-1-0x7fffffffffffffff)

#define INT8_MAX   (0x7f)
#define INT16_MAX  (0x7fff)
#define INT32_MAX  (0x7fffffff)
#define INT64_MAX  (0x7fffffffffffffff)

#define UINT8_MAX  (0xff)
#define UINT16_MAX (0xffff)
#define UINT32_MAX (0xffffffff)
#define UINT64_MAX (0xffffffffffffffff)
#endif

#define WASM_PAGE_SIZE (1024 * 64)

// The code generator compiles in the starting number of wasm pages, and the maximum number of pages
// If we try and allocate more than max_pages, we should fault
extern u32 starting_pages;
extern u32 max_pages;

// Some backends might need to do manual switching when we go into the runtime
INLINE void switch_into_runtime();
INLINE void switch_out_of_runtime();

// The code generator also compiles in stubs that populate the linear memory and function table
void populate_memory();
void populate_table();

// memory/* provides these memory functions
extern void* memory;
extern u32 memory_size;

void alloc_linear_memory();
void expand_memory();
INLINE char* get_memory_ptr_for_runtime(u32 offset, u32 bounds_check);

INLINE i8 get_i8(u32 offset);
INLINE i16 get_i16(u32 offset);
INLINE i32 get_i32(u32 offset);
INLINE i64 get_i64(u32 offset);
INLINE void set_i8(u32 offset, i8 v);
INLINE void set_i16(u32 offset, i16 v);
INLINE void set_i32(u32 offset, i32 v);
INLINE void set_i64(u32 offset, i64 v);

INLINE float get_f32(u32 offset);
INLINE double get_f64(u32 offset);
INLINE void set_f32(u32 offset, float);
INLINE void set_f64(u32 offset, double);

i32 instruction_memory_size();
i32 instruction_memory_grow();

static inline void* get_memory_ptr_void(u32 offset, u32 bounds_check) {
    return (void*) get_memory_ptr_for_runtime(offset, bounds_check);
}

static inline char* get_memory_string(u32 offset) {
    char *naive_ptr = get_memory_ptr_for_runtime(offset, 1);
    int i = 0;
    while (1) {
        // Keep bounds checking the waters over and over until we know it's safe (we find a terminating character)
        char ith_element = get_memory_ptr_for_runtime(offset, i + 1)[i];
        if (ith_element == '\0') {
            return naive_ptr;
        }
        i++;
    }
}

u32 allocate_n_bytes(u32 n);

// memory/* also provides the table access functions
// TODO: Change this to use a compiled in size
#define INDIRECT_TABLE_SIZE 1024

struct indirect_table_entry {
    u32 type_id;
    void* func_pointer;
};

extern struct indirect_table_entry indirect_table[INDIRECT_TABLE_SIZE];

INLINE char* get_function_from_table(u32 idx, u32 type_id);

// libc/* might need to do some setup for the libc setup
void stub_init();

// The runtime entrypoint must be called
int runtime_main(int argc, char** argv);

// Globals used to buffer arguments pending WASI calls
extern u32 runtime_argc;
extern u32 *runtime_argv_buffer_offsets;
extern char *runtime_argv_buffer;
extern u32 runtime_argv_buffer_len;
