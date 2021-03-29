#include "../runtime.h"

u32 memory_size = 0;

#define MEM_SIZE (WASM_PAGE_SIZE * 1 << 2)
char CORTEX_M_MEM[MEM_SIZE + sizeof(u64)] = { 0 };

u32 cortex_mem_size = MEM_SIZE;

int printf_(const char* format, ...);

void alloc_linear_memory() {
    //    printf_("8 = (%d %d) 16 = (%d %d) 32 = (%d %d) 64 = (%d %d)\n", sizeof(u8), sizeof(i8), sizeof(u16),
    //    sizeof(i16), sizeof(u32), sizeof(i32), sizeof(u64), sizeof(i64)); printf_("starting pages = %d, starting
    //    mem size = %d\n", starting_pages, starting_pages * WASM_PAGE_SIZE);
    awsm_assert(MEM_SIZE >= starting_pages * WASM_PAGE_SIZE);
    memory_size = starting_pages * WASM_PAGE_SIZE;
}

void expand_memory() {
    // max_pages = 0 => no limit
    awsm_assert(max_pages == 0 || (memory_size + WASM_PAGE_SIZE <= max_pages * WASM_PAGE_SIZE));

    //    printf_("Expanding to %d\n", memory_size + WASM_PAGE_SIZE);
    awsm_assert(memory_size + WASM_PAGE_SIZE <= sizeof(CORTEX_M_MEM));

    memset(&CORTEX_M_MEM[memory_size], 0, WASM_PAGE_SIZE);
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
    awsm_assert(offset <= memory_size - bounds_check);

    char* address = &CORTEX_M_MEM[offset];
    return address;
}

// All of these are pretty generic
INLINE float get_f32(u32 offset) {
    offset = offset % MEM_SIZE;

    void* address = &CORTEX_M_MEM[offset];
    return *(float*)address;
}

INLINE double get_f64(u32 offset) {
    offset = offset % MEM_SIZE;

    void* address = &CORTEX_M_MEM[offset];
    return *(double*)address;
}

INLINE i8 get_i8(u32 offset) {
    offset = offset % MEM_SIZE;

    void* address = &CORTEX_M_MEM[offset];
    return *(i8*)address;
}

INLINE i16 get_i16(u32 offset) {
    return *(i16*)&CORTEX_M_MEM[offset % MEM_SIZE];
}

INLINE i32 get_i32(u32 offset) {
    return *(i32*)&CORTEX_M_MEM[offset % MEM_SIZE];
}

INLINE i64 get_i64(u32 offset) {
    offset = offset % MEM_SIZE;

    void* address = &CORTEX_M_MEM[offset];
    return *(i64*)address;
}

// Now setting routines
INLINE void set_f32(u32 offset, float v) {
    offset = offset % MEM_SIZE;

    void* address    = &CORTEX_M_MEM[offset];
    *(float*)address = v;
}

INLINE void set_f64(u32 offset, double v) {
    offset = offset % MEM_SIZE;

    void* address     = &CORTEX_M_MEM[offset];
    *(double*)address = v;
}

INLINE void set_i8(u32 offset, i8 v) {
    offset = offset % MEM_SIZE;

    void* address = &CORTEX_M_MEM[offset];
    *(i8*)address = v;
}

INLINE void set_i16(u32 offset, i16 v) {
    offset = offset % MEM_SIZE;

    void* address  = &CORTEX_M_MEM[offset];
    *(i16*)address = v;
}

INLINE void set_i32(u32 offset, i32 v) {
    offset = offset % MEM_SIZE;

    void* address  = &CORTEX_M_MEM[offset];
    *(i32*)address = v;
}

INLINE void set_i64(u32 offset, i64 v) {
    offset = offset % MEM_SIZE;

    void* address  = &CORTEX_M_MEM[offset];
    *(i64*)address = v;
}

INLINE char* get_function_from_table(u32 idx, u32 type_id) {
    awsm_assert(idx < INDIRECT_TABLE_SIZE);

    struct indirect_table_entry f = indirect_table[idx];

    awsm_assert(f.type_id == type_id && f.func_pointer);

    return f.func_pointer;
}

// Functions that aren't useful for this runtime
INLINE void switch_into_runtime() {
    return;
}
INLINE void switch_out_of_runtime() {
    return;
}
