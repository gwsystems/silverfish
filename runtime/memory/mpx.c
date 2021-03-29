#include "../runtime.h"

void* memory;
u32   memory_size;

void alloc_linear_memory() {
    for (u32 i = 0; i < starting_pages; i++) {
        expand_memory();
    }

    asm volatile("bndmk (%0,%1,1), %%bnd0" : : "r"(memory), "r"((intptr_t)memory_size));
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
    assert(memory_size > bounds_check && offset <= memory_size - bounds_check);

    char* mem_as_chars = (char*)memory;
    return &mem_as_chars[offset];
}

#define MPX_BC(adr, sz) \
    { asm volatile("bndcu " #sz "(%0), %%bnd0" : : "r"(adr)); }

// All of these are pretty generic
INLINE float get_f32(u32 offset) {
    char* mem_as_chars = (char*)memory;
    void* address      = &mem_as_chars[offset];

    // Bounds check
    MPX_BC(address, 0x3);

    float v = *(float*)address;
    return v;
}

INLINE double get_f64(u32 offset) {
    char* mem_as_chars = (char*)memory;
    void* address      = &mem_as_chars[offset];

    MPX_BC(address, 0x7);

    double v = *(double*)address;
    return v;
}

INLINE i8 get_i8(u32 offset) {
    char* mem_as_chars = (char*)memory;
    void* address      = &mem_as_chars[offset];

    MPX_BC(address, 0x0);

    return *(i8*)address;
}

INLINE i16 get_i16(u32 offset) {
    char* mem_as_chars = (char*)memory;
    void* address      = &mem_as_chars[offset];

    MPX_BC(address, 0x1);

    return *(i16*)address;
}

INLINE i32 get_i32(u32 offset) {
    char* mem_as_chars = (char*)memory;
    void* address      = &mem_as_chars[offset];

    MPX_BC(address, 0x3);

    i32 v = *(i32*)address;
    return v;
}

INLINE i64 get_i64(u32 offset) {
    char* mem_as_chars = (char*)memory;
    void* address      = &mem_as_chars[offset];

    MPX_BC(address, 0x7);

    i64 v = *(i64*)address;
    return v;
}

// Now setting routines
INLINE void set_f32(u32 offset, float v) {
    char* mem_as_chars = (char*)memory;
    void* address      = &mem_as_chars[offset];

    MPX_BC(address, 0x3);

    *(float*)address = v;
}

INLINE void set_f64(u32 offset, double v) {
    char* mem_as_chars = (char*)memory;
    void* address      = &mem_as_chars[offset];

    MPX_BC(address, 0x7);

    *(double*)address = v;
}

INLINE void set_i8(u32 offset, i8 v) {
    char* mem_as_chars = (char*)memory;
    void* address      = &mem_as_chars[offset];

    MPX_BC(address, 0x0);

    *(i8*)address = v;
}

INLINE void set_i16(u32 offset, i16 v) {
    char* mem_as_chars = (char*)memory;
    void* address      = &mem_as_chars[offset];

    MPX_BC(address, 0x1);

    *(i16*)address = v;
}

INLINE void set_i32(u32 offset, i32 v) {
    char* mem_as_chars = (char*)memory;
    void* address      = &mem_as_chars[offset];

    MPX_BC(address, 0x3);

    *(i32*)address = v;
}

INLINE void set_i64(u32 offset, i64 v) {
    char* mem_as_chars = (char*)memory;
    void* address      = &mem_as_chars[offset];

    MPX_BC(address, 0x7);

    *(i64*)address = v;
}

INLINE char* get_function_from_table(u32 idx, u32 type_id) {
    assert(idx < INDIRECT_TABLE_SIZE);

    struct indirect_table_entry f = indirect_table[idx];

    assert(f.type_id == type_id && f.func_pointer);

    return f.func_pointer;
}

// Functions that aren't useful for this runtime
INLINE void switch_into_runtime() {}
INLINE void switch_out_of_runtime() {}
