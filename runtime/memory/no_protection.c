#include "../runtime.h"

void* memory;
u32   memory_size;

void alloc_linear_memory() {
    memory      = calloc(starting_pages, WASM_PAGE_SIZE);
    memory_size = starting_pages * WASM_PAGE_SIZE;
}

void expand_memory() {
    // max_pages = 0 => no limit
    assert(max_pages == 0 || (memory_size / WASM_PAGE_SIZE < max_pages));

    memory = realloc(memory, memory_size + WASM_PAGE_SIZE);
    assert(memory);

    char* mem_as_chars = memory;
    memset(&mem_as_chars[memory_size], 0, WASM_PAGE_SIZE);
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
    char* mem_as_chars = (char*)memory;
    return &mem_as_chars[offset];
}

// All of these are pretty generic
INLINE float get_f32(u32 offset) {
    char* mem_as_chars = (char*)memory;
    void* address      = &mem_as_chars[offset];
    float v            = *(float*)address;
    return v;
}

INLINE double get_f64(u32 offset) {
    char*  mem_as_chars = (char*)memory;
    void*  address      = &mem_as_chars[offset];
    double v            = *(double*)address;
    return v;
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
    i32   v            = *(i32*)address;
    return v;
}

INLINE i64 get_i64(u32 offset) {
    char* mem_as_chars = (char*)memory;
    void* address      = &mem_as_chars[offset];
    i64   v            = *(i64*)address;
    return v;
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

INLINE char* get_function_from_table(u32 idx, u32 type_id) {
    struct indirect_table_entry f = indirect_table[idx];
    return f.func_pointer;
}

// Functions that aren't useful for this runtime
INLINE void switch_into_runtime() {}
INLINE void switch_out_of_runtime() {}
