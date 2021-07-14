(module
    (import "wasi_snapshot_preview1" "args_get"       (func $args_get (param i32 i32) (result i32)))
    (import "wasi_snapshot_preview1" "args_sizes_get" (func $args_sizes_get (param i32 i32) (result i32)))
    (import "wasi_snapshot_preview1" "fd_write"       (func $fd_write (param i32 i32 i32 i32) (result i32)))
    (import "wasi_snapshot_preview1" "proc_exit"      (func $proc_exit (param i32)))
    
    (memory 2)
    (export "memory" (memory 0))

    ;; TODO: This is required to run under awsm
    ;; But this shouldn't be a hard requirement
    (table $tbl 0 anyfunc)

    ;; storing linear memory offsets as globals
    (global $stdout i32 (i32.const 1))
    (global $argc i32 (i32.const 0))
    (global $args_len i32 (i32.const 4))

    ;; This is a vector of offsets for arguments inside of the buffer of length argc
    (global $args_p i32 (i32.const 8))
    ;; gap for up to space for 14 arguments
    
    (global $args_buffer i32 (i32.const 64))
    ;; gap for up to 136 characters

    ;; Buffer space set prior to calling fd_write
    (global $iov_base i32 (i32.const 200)) ;; iov struct first member
    (global $iov_len i32 (i32.const 204))  ;; iov struct second member
    (global $nchars i32 (i32.const 208))
    
    ;; newline string
    (global $newline i32 (i32.const 1000))
    (data (i32.const 1000) "\n")

    (func $print_args
        ;; Write the contents of the arguments buffer
        ;; The args are delimited by \0 chars, but these chars likely do not actually print to console
        ;; so "first\0second" looks like "firstsecond"
        (i32.store (global.get $iov_base) (i32.load (global.get $args_p)))  ;; iov.iov_base 
        (i32.store (global.get $iov_len) (i32.load (global.get $args_len)))  ;; iov.iov_len 
        (call $fd_write
            (global.get $stdout) ;; target file_descriptor
            (global.get $iov_base) ;; *iovs - The pointer to the iov array, which is stored at memory location 0
            (i32.const 1) ;; iovs_len - We're printing 1 string stored in an iov - so one.
            (global.get $nchars) ;; nwritten - A place in memory to store the number of bytes written
        )
        drop ;; Discard the number of bytes written from the top of the stack

        ;; Write trailing newline
        (i32.store (global.get $iov_base) (global.get $newline))  ;; iov.iov_base 
        (i32.store (global.get $iov_len) (i32.const 2))  ;; iov.iov_len 
        (call $fd_write
            (global.get $stdout) ;; target file_descriptor
            (global.get $iov_base) ;; *iovs - The pointer to the iov array, which is stored at memory location 0
            (i32.const 1) ;; iovs_len - We're printing 1 string stored in an iov - so one.
            (global.get $nchars) ;; nwritten - A place in memory to store the number of bytes written
        )
        drop ;; Discard the number of bytes written from the top of the stack
    )

    (func $_start (export "_start")
        (call $args_sizes_get (global.get $argc) (global.get $args_len))
        (call $args_get (global.get $args_p) (global.get $args_buffer))
        drop
        call $print_args
        drop

        (call $proc_exit (i32.const 0))
    )
)
