#include "../runtime.h"

#include <syscall.h>

#include <asm/ldt.h>
#include <sys/mman.h>

// Routines for dealing with the ldt
void write_ldt(struct user_desc* desc) {
    int i = syscall(__NR_modify_ldt, 1, desc, sizeof(*desc));
    assert(i == 0);
}

#define GS_IDX 10
int stored_gs_val = 0;

int read_gs() {
    int i;
    asm volatile("movl %%gs, %0" : "=r"(i));
    return i;
}

void write_gs_raw(int v) {
    asm volatile("movl %0, %%gs" : : "r"(v) : "memory");
}

void write_gs(int idx) {
    int v = (idx << 3) | 0x7;
    write_gs_raw(v);
}

void* memory;
u32   memory_size;

inline static void set_seg_registers() {
    stored_gs_val = read_gs();
    write_gs(GS_IDX);
}

inline static void reset_seg_registers() {
    write_gs_raw(stored_gs_val);
}

void alloc_linear_memory() {
    memory      = calloc(starting_pages, WASM_PAGE_SIZE);
    memory_size = starting_pages * WASM_PAGE_SIZE;

    struct user_desc gs_desc = (struct user_desc){
        .entry_number    = GS_IDX,
        .base_addr       = (int)memory,
        .limit           = memory_size / 4096,
        .seg_32bit       = 1, // TODO: Make sure this makes sense
        .contents        = 0,
        .read_exec_only  = 0,
        .limit_in_pages  = 1,
        .seg_not_present = 0,
        .useable         = 1,
    };
    write_ldt(&gs_desc);
}

void expand_memory() {
    // max_pages = 0 => no limit
    reset_seg_registers();
    assert(max_pages == 0 || (memory_size / WASM_PAGE_SIZE < max_pages));

    memory = realloc(memory, memory_size + WASM_PAGE_SIZE);
    assert(memory);

    char* mem_as_chars = memory;
    memset(&mem_as_chars[memory_size], 0, WASM_PAGE_SIZE);
    memory_size += WASM_PAGE_SIZE;

    struct user_desc gs_desc = (struct user_desc){
        .entry_number    = GS_IDX,
        .base_addr       = (int)memory,
        .limit           = memory_size / 4096,
        .seg_32bit       = 1, // TODO: Make sure this makes sense
        .contents        = 0,
        .read_exec_only  = 0,
        .limit_in_pages  = 1,
        .seg_not_present = 0,
        .useable         = 1,
    };
    write_ldt(&gs_desc);

    set_seg_registers();
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
    char* address      = &mem_as_chars[offset];

    return address;
}

#define GS_REL __attribute__((address_space(256)))

// All of these are pretty generic
INLINE float get_f32(u32 offset) {
    return *((GS_REL float*)offset);
}

INLINE double get_f64(u32 offset) {
    return *((GS_REL double*)offset);
}

INLINE i8 get_i8(u32 offset) {
    return *((GS_REL i8*)offset);
}

INLINE i16 get_i16(u32 offset) {
    return *((GS_REL i16*)offset);
}

INLINE i32 get_i32(u32 offset) {
    return *((GS_REL i32*)offset);
}

INLINE i64 get_i64(u32 offset) {
    return *((GS_REL i64*)offset);
}

// Now setting routines
INLINE void set_f32(u32 offset, float v) {
    GS_REL float* ptr = (GS_REL float*)offset;
    *ptr              = v;
}

INLINE void set_f64(u32 offset, double v) {
    GS_REL double* ptr = (GS_REL double*)offset;
    *ptr               = v;
}

INLINE void set_i8(u32 offset, i8 v) {
    GS_REL i8* ptr = (GS_REL i8*)offset;
    *ptr           = v;
}

INLINE void set_i16(u32 offset, i16 v) {
    GS_REL i16* ptr = (GS_REL i16*)offset;
    *ptr            = v;
}

INLINE void set_i32(u32 offset, i32 v) {
    GS_REL i32* ptr = (GS_REL i32*)offset;
    *ptr            = v;
}

INLINE void set_i64(u32 offset, i64 v) {
    GS_REL i64* ptr = (GS_REL i64*)offset;
    *ptr            = v;
}

INLINE char* get_function_from_table(u32 idx, u32 type_id) {
    assert(idx < INDIRECT_TABLE_SIZE);

    struct indirect_table_entry f = indirect_table[idx];

    assert(f.type_id == type_id && f.func_pointer);

    return f.func_pointer;
}

INLINE void switch_into_runtime() {
    reset_seg_registers();
}

INLINE void switch_out_of_runtime() {
    set_seg_registers();
}
