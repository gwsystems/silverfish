#include "runtime.h"

// TODO: Throughout here we use `assert` for error conditions, which isn't optimal
// Instead we should use `unlikely` branches to a single trapping function (which should optimize better)

// Needed to support C++
void env___cxa_pure_virtual() { awsm_assert("env___cxa_pure_virtual" == 0); }

// Region initialization helper function
EXPORT void initialize_region(u32 offset, u32 data_count, char* data) {
    awsm_assert(memory_size >= data_count);
    awsm_assert(offset < memory_size - data_count);
//    awsm_assert(offset <= memory_size - data_count);

    // FIXME: Hack around segmented and unsegmented access
    memcpy(get_memory_ptr_for_runtime(offset, data_count), data, data_count);
}

struct indirect_table_entry indirect_table[INDIRECT_TABLE_SIZE];

void add_function_to_table(u32 idx, u32 type_id, char* pointer) {
    awsm_assert(idx < INDIRECT_TABLE_SIZE);
    indirect_table[idx] = (struct indirect_table_entry) { .type_id = type_id, .func_pointer = pointer };
}

void clear_table() {
    for (int i = 0; i < INDIRECT_TABLE_SIZE; i++) {
        indirect_table[i] = (struct indirect_table_entry) { 0 };
    }
}

// The below functions are for implementing WASM instructions

// ROTL and ROTR helper functions
INLINE u32 rotl_u32(u32 n, u32 c_u32) {
    // WASM requires a modulus here (usually a single bitwise op, but it means we need no assert)
    unsigned int c = c_u32 % (CHAR_BIT * sizeof(n));
    const unsigned int mask = (CHAR_BIT * sizeof(n) - 1);  // assumes width is a power of 2.

    c &= mask;
    return (n << c) | (n >> ((-c) & mask));
}

INLINE u32 rotr_u32(u32 n, u32 c_u32) {
    // WASM requires a modulus here (usually a single bitwise op, but it means we need no assert)
    unsigned int c = c_u32 % (CHAR_BIT * sizeof(n));
    const unsigned int mask = (CHAR_BIT * sizeof(n) - 1);

    c &= mask;
    return (n>>c) | (n << ((-c) & mask));
}

INLINE u64 rotl_u64(u64 n, u64 c_u64) {
    // WASM requires a modulus here (usually a single bitwise op, but it means we need no assert)
    unsigned int c = c_u64 % (CHAR_BIT * sizeof(n));
    const unsigned int mask = (CHAR_BIT * sizeof(n) - 1);  // assumes width is a power of 2.

    c &= mask;
    return (n << c) | (n >> ((-c) & mask));
}

INLINE u64 rotr_u64(u64 n, u64 c_u64) {
    // WASM requires a modulus here (usually a single bitwise op, but it means we need no assert)
    unsigned int c = c_u64 % (CHAR_BIT * sizeof(n));
    const unsigned int mask = (CHAR_BIT * sizeof(n) - 1);

    c &= mask;
    return (n >> c) | (n << ((-c) & mask));
}

// Now safe division and remainder
INLINE u32 u32_div(u32 a, u32 b) {
    awsm_assert(b);
    return a / b;
}

INLINE u32 u32_rem(u32 a, u32 b) {
    awsm_assert(b);
    return a % b;
}

INLINE i32 i32_div(i32 a, i32 b) {
    awsm_assert(b && (a != INT32_MIN || b != -1));
    return a / b;
}

INLINE i32 i32_rem(i32 a, i32 b) {
    awsm_assert(b && (a != INT32_MIN || b != -1));
    return a % b;
}

INLINE u64 u64_div(u64 a, u64 b) {
    awsm_assert(b);
    return a / b;
}

INLINE u64 u64_rem(u64 a, u64 b) {
    awsm_assert(b);
    return a % b;
}

INLINE i64 i64_div(i64 a, i64 b) {
    awsm_assert(b && (a != INT64_MIN || b != -1));
    return a / b;
}

INLINE i64 i64_rem(i64 a, i64 b) {
    awsm_assert(b && (a != INT64_MIN || b != -1));
    return a % b;
}



// float to integer conversion methods
// In C, float => int conversions always truncate
// If a int2float(int::min_value) <= float <= int2float(int::max_value), it must always be safe to truncate it
u32 u32_trunc_f32(float f) {
    awsm_assert(0 <= f && f <= (float) UINT32_MAX);
    return (u32) f;
}

i32 i32_trunc_f32(float f) {
    awsm_assert(INT32_MIN <= f && f <= (float) INT32_MAX);
    return (i32) f;
}

u32 u32_trunc_f64(double f) {
    awsm_assert(0 <= f && f <= (float) UINT32_MAX);
    return (u32) f;
}

i32 i32_trunc_f64(double f) {
    awsm_assert(INT32_MIN <= f && f <= (float) INT32_MAX );
    return (i32) f;
}

u64 u64_trunc_f32(float f) {
    awsm_assert(0 <= f && f <= (float) UINT64_MAX);
    return (u64) f;
}

i64 i64_trunc_f32(float f) {
    awsm_assert(INT64_MIN <= f && f <= (float) INT64_MAX);
    return (i64) f;
}

u64 u64_trunc_f64(double f) {
    awsm_assert(0 <= f && f <= (double) UINT64_MAX);
    return (u64) f;
}

i64 i64_trunc_f64(double f) {
    awsm_assert(INT64_MIN <= f && f <= (double) INT64_MAX);
    return (i64) f;
}

// Float => Float truncation functions
INLINE float f32_trunc_f32(float f) {
    return trunc(f);
}

INLINE float f32_min(float a, float b) {
    return a < b ? a : b;
}

INLINE float f32_max(float a, float b) {
    return a > b ? a : b;
}

INLINE float f32_floor(float a) {
    return floorf(a);
}

INLINE float f32_ceil(float a) {
    return ceil(a);
}

INLINE float f32_nearest(float a) {
    return round(a);
}

INLINE float f32_copysign(float a, float b) {
    return copysignf(a, b);
}

INLINE double f64_min(double a, double b) {
    return a < b ? a : b;
}

INLINE double f64_max(double a, double b) {
    return a > b ? a : b;
}

INLINE double f64_floor(double a) {
    return floor(a);
}

INLINE double f64_ceil(double a) {
    return ceil(a);
}

INLINE double f64_nearest(double a) {
    return round(a);
}

INLINE double f64_copysign(double a, double b) {
    return copysign(a, b);
}

// We want to have some allocation logic here, so we can use it to implement libc
WEAK u32 wasmg___heap_base = 0;
u32 runtime_heap_base;

u32 allocate_n_bytes(u32 n) {
    u32 res = runtime_heap_base;
    runtime_heap_base += n;
    while (memory_size < runtime_heap_base) {
        expand_memory();
    }
    printf("rhb %d\n", runtime_heap_base);
    return res;
}

void* allocate_n_bytes_ptr(u32 n) {
    u32 addr = allocate_n_bytes(n);
    return get_memory_ptr_for_runtime(addr, n);
}

// FIXME: Currently the timer support stuff is disabled, pending cortex_m results
// this provides a way to add a timeout for wasm execution, and support for that
// TODO: Add a way for awsm to give us this value
//WEAK unsigned int wasm_execution_timeout_ms = 0;
//sigjmp_buf timeout_jump;
//#define JUMPING_BACK 0xBACC

// Precondition: you've already loaded timeout_jump with where we should jump after the timeout
//void handle_sigalarm(int sig) {
//    // TODO: We use siglongjmp which resets signal stuff, so perhaps this call is unnessesary
//    signal(sig, SIG_IGN);
//    siglongjmp(timeout_jump, JUMPING_BACK);
//}
//
//void schedule_timeout() {
//    signal(SIGALRM, handle_sigalarm);
//    ualarm(wasm_execution_timeout_ms * 1000, 0);
//}
//
//void cancel_timeout() {
//    ualarm(0, 0);
//}

// If we are using runtime globals, we need to populate them
WEAK void populate_globals() {}

// Code that actually runs the wasm code
IMPORT void wasmf__start(void);

u32 runtime_argc = 0;

char *runtime_argv_buffer = NULL;
u32 runtime_argv_buffer_len = 0;

u32 *runtime_argv_buffer_offsets = NULL;

void runtime_cleanup() {
    // Cancel any pending timeout
    //    if (wasm_execution_timeout_ms) {
    //        cancel_timeout();
    //    }

    free(runtime_argv_buffer);
    runtime_argv_buffer_len = 0;
    free(runtime_argv_buffer_offsets);
}

int runtime_main(int argc, char** argv) {
    // Set argc and argv to globals, these are later used by the WASI syscalls 
    runtime_argc = argc;

    runtime_argv_buffer_offsets = calloc(argc, sizeof(u32));

    // Copy the null terminated args one after the other into a global with the format WASI calls expect
    for (int i = 0; i < argc; i++) {
        runtime_argv_buffer_offsets[i] = runtime_argv_buffer_len;
        size_t arg_len = strlen(argv[i]) + 1;
        runtime_argv_buffer = realloc(runtime_argv_buffer, runtime_argv_buffer_len + arg_len);
        strcpy(&runtime_argv_buffer[runtime_argv_buffer_len], argv[i]);
        runtime_argv_buffer_len += (int)arg_len;
    }

    // Setup the linear memory and function table
    alloc_linear_memory();
    populate_table();

    // Setup our allocation logic
    runtime_heap_base = wasmg___heap_base;
    printf("starting rhb %d\n", runtime_heap_base);
    if (runtime_heap_base == 0) {
        runtime_heap_base = memory_size;
    }

    // Setup the global values (if needed), and populate the linear memory
    switch_out_of_runtime();
    populate_globals();
    switch_into_runtime();
    populate_memory();

    // In the case of a real timeout being compiled in, handle that
//    if (wasm_execution_timeout_ms) {
//        // Set the jumpoint to here, and save the signal mask
//        int res = sigsetjmp(timeout_jump, 1);
//        if (res != 0) {
//            assert(res == JUMPING_BACK);
//            printf("WE DECIDED TO GIVE UP\n");
//            return -JUMPING_BACK;
//        }
//        schedule_timeout();
//    }

    stub_init();
    atexit(runtime_cleanup);

    switch_out_of_runtime();
    wasmf__start();
    switch_into_runtime();

    // TODO: Improve "implicit exit" path
    // My understanding is that _start can optionally return. How can we get this?
    return 0;
}
