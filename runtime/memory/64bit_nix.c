#include "../runtime.h"

#include <sys/mman.h>

void* memory      = NULL;
u32   memory_size = 0;

void alloc_linear_memory() {
    // Map 4gb + PAGE_SIZE of memory that will fault when accessed
    // We allocate the extra page so that reads off the end will also fail
    memory = mmap(NULL, (1LL << 32) + WASM_PAGE_SIZE, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED) {
        perror("Mapping of initial unusable region failed");
        exit(1);
    }

    void* map_result = mmap(memory, WASM_PAGE_SIZE * starting_pages, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (map_result == MAP_FAILED) {
        perror("Mapping of initial memory failed");
        exit(1);
    }
    memory_size = WASM_PAGE_SIZE * starting_pages;
}

void expand_memory() {
    // max_pages = 0 => no limit
    awsm_assert(max_pages == 0 || (memory_size / WASM_PAGE_SIZE < max_pages));
    // Remap the relevant wasm page to readable
    char* mem_as_chars = memory;
    char* page_address = &mem_as_chars[memory_size];

    void* map_result = mmap(page_address, WASM_PAGE_SIZE, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (map_result == MAP_FAILED) {
        perror("Mapping of new memory failed");
        exit(1);
    }
    memory_size += WASM_PAGE_SIZE;
}

i32 instruction_memory_size() {
    return memory_size / WASM_PAGE_SIZE;
}

i32 instruction_memory_grow(i32 count) {
    i32 prev_size = instruction_memory_size();
    for (int i = 0; i < count; i++) {
        expand_memory();
    }

    return prev_size;
}

INLINE char* get_memory_ptr_for_runtime(u32 offset, u32 bounds_check) {
    // Due to how we setup memory for x86, the virtual memory mechanism will catch the error, if bounds <
    // WASM_PAGE_SIZE
    assert(bounds_check < WASM_PAGE_SIZE || (memory_size > bounds_check && offset <= memory_size - bounds_check));

    char* mem_as_chars = (char*)memory;
    char* address      = &mem_as_chars[offset];
    return address;
}

// All of these are pretty generic
INLINE float get_f32(u32 offset) {
    char* mem_as_chars = (char*)memory;
    void* address      = &mem_as_chars[offset];
    return *(float*)address;
}

INLINE double get_f64(u32 offset) {
    char* mem_as_chars = (char*)memory;
    void* address      = &mem_as_chars[offset];
    return *(double*)address;
}

INLINE i8 get_i8(u32 offset) {
    char* mem_as_chars = (char*)memory;
    void* address      = &mem_as_chars[offset];
    return *(i8*)address;
}

INLINE i16 get_i16(u32 offset) {
    char* mem_as_chars = (char*)memory;
    void* address      = &mem_as_chars[offset];
    return *(i16*)address;
}

INLINE i32 get_i32(u32 offset) {
    char* mem_as_chars = (char*)memory;
    void* address      = &mem_as_chars[offset];
    return *(i32*)address;
}

INLINE i64 get_i64(u32 offset) {
    char* mem_as_chars = (char*)memory;
    void* address      = &mem_as_chars[offset];
    return *(i64*)address;
}

// Now setting routines
INLINE void set_f32(u32 offset, float v) {
    char* mem_as_chars = (char*)memory;
    void* address      = &mem_as_chars[offset];
    *(float*)address   = v;
}

INLINE void set_f64(u32 offset, double v) {
    char* mem_as_chars = (char*)memory;
    void* address      = &mem_as_chars[offset];
    *(double*)address  = v;
}

INLINE void set_i8(u32 offset, i8 v) {
    char* mem_as_chars = (char*)memory;
    void* address      = &mem_as_chars[offset];
    *(i8*)address      = v;
}

INLINE void set_i16(u32 offset, i16 v) {
    char* mem_as_chars = (char*)memory;
    void* address      = &mem_as_chars[offset];
    *(i16*)address     = v;
}

INLINE void set_i32(u32 offset, i32 v) {
    char* mem_as_chars = (char*)memory;
    void* address      = &mem_as_chars[offset];
    *(i32*)address     = v;
}

INLINE void set_i64(u32 offset, i64 v) {
    char* mem_as_chars = (char*)memory;
    void* address      = &mem_as_chars[offset];
    *(i64*)address     = v;
}

// Table handling functionality
INLINE char* get_function_from_table(u32 idx, u32 type_id) {
    assert(idx < INDIRECT_TABLE_SIZE);

    struct indirect_table_entry f = indirect_table[idx];

    // NOTE: Legacy C applications could fail this check if they typecast function pointers.
    // Additional reference: https://emscripten.org/docs/porting/guidelines/function_pointer_issues.html
    assert(f.type_id == type_id);
    assert(f.func_pointer);

    return f.func_pointer;
}

// Functions that aren't useful for this runtime
INLINE void switch_into_runtime() {}
INLINE void switch_out_of_runtime() {}
