#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <printf.h>
#include <setjmp.h>
#include <signal.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/uio.h>

#include "../runtime.h"

/* POSIX compatibility shims */
#ifndef O_RSYNC
#define O_RSYNC O_SYNC
#endif

#ifdef __APPLE__
#undef fdatasync
#define fdatasync fsync
#endif

int main(int argc, char* argv[]) {
    runtime_main(argc, argv);
    printf("mem use = %d\n", (int) memory_size);
}

// Called before wasmf_main
void stub_init() {
    switch_out_of_runtime();
    // wasmf___wasm_call_ctors();
    switch_into_runtime();
}
// WASI API implementations

// some definitions we need
struct wasi_iovec {
    i32 base_offset;
    i32 len;
};

struct wasi_fdstat {
    u8  fs_filetype;
    u16 fs_flags;
    u64 fs_rights_base;
    u64 fs_rights_inheriting;
};

struct wasi_filestat {
    u64 st_dev;
    u64 st_ino;
    u8 st_filetype;
    u32 st_nlink;
    u64 st_size;
    u64 st_atim;
    u64 st_mtim;
    u64 st_ctim;
};

enum wasi_filetype {
    WASI_FILETYPE_UNKNOWN           = 0,
    WASI_FILETYPE_BLOCK_DEVICE      = 1,
    WASI_FILETYPE_CHARACTER_DEVICE  = 2,
    WASI_FILETYPE_DIRECTORY         = 3,
    WASI_FILETYPE_REGULAR_FILE      = 4,
    WASI_FILETYPE_SOCKET_DGRAM      = 5,
    WASI_FILETYPE_SOCKET_STREAM     = 6,
    WASI_FILETYPE_SYMBOLIC_LINK     = 7,
};

enum wasi_oflags {
    WASI_O_CREAT        = 0x0001,
    WASI_O_DIRECTORY    = 0x0002,
    WASI_O_EXCL         = 0x0004,
    WASI_O_TRUNC        = 0x0008,
};

enum wasi_fdflags {
    WASI_FDFLAG_APPEND      = 0x0001,
    WASI_FDFLAG_DSYNC       = 0x0002,
    WASI_FDFLAG_NONBLOCK    = 0x0004,
    WASI_FDFLAG_RSYNC       = 0x0008,
    WASI_FDFLAG_SYNC        = 0x0010,
};

enum wasi_rights {
    WASI_RIGHT_FD_DATASYNC              = 0x0000000000000001ULL,
    WASI_RIGHT_FD_READ                  = 0x0000000000000002ULL,
    WASI_RIGHT_FD_SEEK                  = 0x0000000000000004ULL,
    WASI_RIGHT_FD_FDSTAT_SET_FLAGS      = 0x0000000000000008ULL,
    WASI_RIGHT_FD_SYNC                  = 0x0000000000000010ULL,
    WASI_RIGHT_FD_TELL                  = 0x0000000000000020ULL,
    WASI_RIGHT_FD_WRITE                 = 0x0000000000000040ULL,
    WASI_RIGHT_FD_ADVISE                = 0x0000000000000080ULL,
    WASI_RIGHT_FD_ALLOCATE              = 0x0000000000000100ULL,
    WASI_RIGHT_PATH_CREATE_DIRECTORY    = 0x0000000000000200ULL,
    WASI_RIGHT_PATH_CREATE_FILE         = 0x0000000000000400ULL,
    WASI_RIGHT_PATH_LINK_SOURCE         = 0x0000000000000800ULL,
    WASI_RIGHT_PATH_LINK_TARGET         = 0x0000000000001000ULL,
    WASI_RIGHT_PATH_OPEN                = 0x0000000000002000ULL,
    WASI_RIGHT_FD_READDIR               = 0x0000000000004000ULL,
    WASI_RIGHT_PATH_READLINK            = 0x0000000000008000ULL,
    WASI_RIGHT_PATH_RENAME_SOURCE       = 0x0000000000010000ULL,
    WASI_RIGHT_PATH_RENAME_TARGET       = 0x0000000000020000ULL,
    WASI_RIGHT_PATH_FILESTAT_GET        = 0x0000000000040000ULL,
    WASI_RIGHT_PATH_FILESTAT_SET_SIZE   = 0x0000000000080000ULL,
    WASI_RIGHT_PATH_FILESTAT_SET_TIMES  = 0x0000000000100000ULL,
    WASI_RIGHT_FD_FILESTAT_GET          = 0x0000000000200000ULL,
    WASI_RIGHT_FD_FILESTAT_SET_SIZE     = 0x0000000000400000ULL,
    WASI_RIGHT_FD_FILESTAT_SET_TIMES    = 0x0000000000800000ULL,
    WASI_RIGHT_PATH_SYMLINK             = 0x0000000001000000ULL,
    WASI_RIGHT_PATH_REMOVE_DIRECTORY    = 0x0000000002000000ULL,
    WASI_RIGHT_PATH_UNLINK_FILE         = 0x0000000004000000ULL,
    WASI_RIGHT_POLL_FD_READWRITE        = 0x0000000008000000ULL,
    WASI_RIGHT_SOCK_SHUTDOWN            = 0x0000000010000000ULL,
};

enum wasi_errnos {
    WASI_ESUCCESS           = 0,
    WASI_E2BIG              = 1,
    WASI_EACCES             = 2,
    WASI_EADDRINUSE         = 3,
    WASI_EADDRNOTAVAIL      = 4,
    WASI_EAFNOSUPPORT       = 5,
    WASI_EAGAIN             = 6,
    WASI_EALREADY           = 7,
    WASI_EBADF              = 8,
    WASI_EBADMSG            = 9,
    WASI_EBUSY              = 10,
    WASI_ECANCELED          = 11,
    WASI_ECHILD             = 12,
    WASI_ECONNABORTED       = 13,
    WASI_ECONNREFUSED       = 14,
    WASI_ECONNRESET         = 15,
    WASI_EDEADLK            = 16,
    WASI_EDESTADDRREQ       = 17,
    WASI_EDOM               = 18,
    WASI_EDQUOT             = 19,
    WASI_EEXIST             = 20,
    WASI_EFAULT             = 21,
    WASI_EFBIG              = 22,
    WASI_EHOSTUNREACH       = 23,
    WASI_EIDRM              = 24,
    WASI_EILSEQ             = 25,
    WASI_EINPROGRESS        = 26,
    WASI_EINTR              = 27,
    WASI_EINVAL             = 28,
    WASI_EIO                = 29,
    WASI_EISCONN            = 30,
    WASI_EISDIR             = 31,
    WASI_ELOOP              = 32,
    WASI_EMFILE             = 33,
    WASI_EMLINK             = 34,
    WASI_EMSGSIZE           = 35,
    WASI_EMULTIHOP          = 36,
    WASI_ENAMETOOLONG       = 37,
    WASI_ENETDOWN           = 38,
    WASI_ENETRESET          = 39,
    WASI_ENETUNREACH        = 40,
    WASI_ENFILE             = 41,
    WASI_ENOBUFS            = 42,
    WASI_ENODEV             = 43,
    WASI_ENOENT             = 44,
    WASI_ENOEXEC            = 45,
    WASI_ENOLCK             = 46,
    WASI_ENOLINK            = 47,
    WASI_ENOMEM             = 48,
    WASI_ENOMSG             = 49,
    WASI_ENOPROTOOPT        = 50,
    WASI_ENOSPC             = 51,
    WASI_ENOSYS             = 52,
    WASI_ENOTCONN           = 53,
    WASI_ENOTDIR            = 54,
    WASI_ENOTEMPTY          = 55,
    WASI_ENOTRECOVERABLE    = 56,
    WASI_ENOTSOCK           = 57,
    WASI_ENOTSUP            = 58,
    WASI_ENOTTY             = 59,
    WASI_ENXIO              = 60,
    WASI_EOVERFLOW          = 61,
    WASI_EOWNERDEAD         = 62,
    WASI_EPERM              = 63,
    WASI_EPIPE              = 64,
    WASI_EPROTO             = 65,
    WASI_EPROTONOSUPPORT    = 66,
    WASI_EPROTOTYPE         = 67,
    WASI_ERANGE             = 68,
    WASI_EROFS              = 69,
    WASI_ESPIPE             = 70,
    WASI_ESRCH              = 71,
    WASI_ESTALE             = 72,
    WASI_ETIMEDOUT          = 73,
    WASI_ETXTBSY            = 74,
    WASI_EXDEV              = 75,
    WASI_ENOTCAPABLE        = 76,
};

// errno code handling
static i32 wasi_fromerrno(int errno_) {
    switch (errno_) {
        case 0:                 return WASI_ESUCCESS;
        case E2BIG:             return WASI_E2BIG;
        case EACCES:            return WASI_EACCES;
        case EADDRINUSE:        return WASI_EADDRINUSE;
        case EADDRNOTAVAIL:     return WASI_EADDRNOTAVAIL;
        case EAFNOSUPPORT:      return WASI_EAFNOSUPPORT;
        case EAGAIN:            return WASI_EAGAIN;
        case EALREADY:          return WASI_EALREADY;
        case EBADF:             return WASI_EBADF;
        case EBADMSG:           return WASI_EBADMSG;
        case EBUSY:             return WASI_EBUSY;
        case ECANCELED:         return WASI_ECANCELED;
        case ECHILD:            return WASI_ECHILD;
        case ECONNABORTED:      return WASI_ECONNABORTED;
        case ECONNREFUSED:      return WASI_ECONNREFUSED;
        case ECONNRESET:        return WASI_ECONNRESET;
        case EDEADLK:           return WASI_EDEADLK;
        case EDESTADDRREQ:      return WASI_EDESTADDRREQ;
        case EDOM:              return WASI_EDOM;
        case EDQUOT:            return WASI_EDQUOT;
        case EEXIST:            return WASI_EEXIST;
        case EFAULT:            return WASI_EFAULT;
        case EFBIG:             return WASI_EFBIG;
        case EHOSTUNREACH:      return WASI_EHOSTUNREACH;
        case EIDRM:             return WASI_EIDRM;
        case EILSEQ:            return WASI_EILSEQ;
        case EINPROGRESS:       return WASI_EINPROGRESS;
        case EINTR:             return WASI_EINTR;
        case EINVAL:            return WASI_EINVAL;
        case EIO:               return WASI_EIO;
        case EISCONN:           return WASI_EISCONN;
        case EISDIR:            return WASI_EISDIR;
        case ELOOP:             return WASI_ELOOP;
        case EMFILE:            return WASI_EMFILE;
        case EMLINK:            return WASI_EMLINK;
        case EMSGSIZE:          return WASI_EMSGSIZE;
        case EMULTIHOP:         return WASI_EMULTIHOP;
        case ENAMETOOLONG:      return WASI_ENAMETOOLONG;
        case ENETDOWN:          return WASI_ENETDOWN;
        case ENETRESET:         return WASI_ENETRESET;
        case ENETUNREACH:       return WASI_ENETUNREACH;
        case ENFILE:            return WASI_ENFILE;
        case ENOBUFS:           return WASI_ENOBUFS;
        case ENODEV:            return WASI_ENODEV;
        case ENOENT:            return WASI_ENOENT;
        case ENOEXEC:           return WASI_ENOEXEC;
        case ENOLCK:            return WASI_ENOLCK;
        case ENOLINK:           return WASI_ENOLINK;
        case ENOMEM:            return WASI_ENOMEM;
        case ENOMSG:            return WASI_ENOMSG;
        case ENOPROTOOPT:       return WASI_ENOPROTOOPT;
        case ENOSPC:            return WASI_ENOSPC;
        case ENOSYS:            return WASI_ENOSYS;
        case ENOTCONN:          return WASI_ENOTCONN;
        case ENOTDIR:           return WASI_ENOTDIR;
        case ENOTEMPTY:         return WASI_ENOTEMPTY;
        case ENOTRECOVERABLE:   return WASI_ENOTRECOVERABLE;
        case ENOTSOCK:          return WASI_ENOTSOCK;
        case ENOTSUP:           return WASI_ENOTSUP;
        case ENOTTY:            return WASI_ENOTTY;
        case ENXIO:             return WASI_ENXIO;
        case EOVERFLOW:         return WASI_EOVERFLOW;
        case EOWNERDEAD:        return WASI_EOWNERDEAD;
        case EPERM:             return WASI_EPERM;
        case EPIPE:             return WASI_EPIPE;
        case EPROTO:            return WASI_EPROTO;
        case EPROTONOSUPPORT:   return WASI_EPROTONOSUPPORT;
        case EPROTOTYPE:        return WASI_EPROTOTYPE;
        case ERANGE:            return WASI_ERANGE;
        case EROFS:             return WASI_EROFS;
        case ESPIPE:            return WASI_ESPIPE;
        case ESRCH:             return WASI_ESRCH;
        case ESTALE:            return WASI_ESTALE;
        case ETIMEDOUT:         return WASI_ETIMEDOUT;
        case ETXTBSY:           return WASI_ETXTBSY;
        case EXDEV:             return WASI_EXDEV;
        default:
            fprintf(stderr, "wasi_fromerrno unexpectedly received: %s\n", strerror(errno_));
            fflush(stderr);
    }

    awsm_assert(0);
    return 0;
}

// file operations
i32 wasi_snapshot_preview1_path_open(
        i32 dirfd,
        u32 lookupflags,
        u32 path_off,
        u32 path_len,
        u32 oflags,
        u64 fs_rights_base,
        u64 fs_rights_inheriting,
        u32 fdflags,
        u32 fd_off) {
    // get path
    char* path = get_memory_string(path_off);

    // translate o_flags and fs_flags into flags and mode
    int flags = (
            ((oflags & WASI_O_CREAT    ) ? O_CREAT     : 0) |
            ((oflags & WASI_O_DIRECTORY) ? O_DIRECTORY : 0) |
            ((oflags & WASI_O_EXCL     ) ? O_EXCL      : 0) |
            ((oflags & WASI_O_TRUNC    ) ? O_TRUNC     : 0) |
            ((fdflags & WASI_FDFLAG_APPEND  ) ? O_APPEND   : 0) |
            ((fdflags & WASI_FDFLAG_DSYNC   ) ? O_DSYNC    : 0) |
            ((fdflags & WASI_FDFLAG_NONBLOCK) ? O_NONBLOCK : 0) |
            ((fdflags & WASI_FDFLAG_RSYNC   ) ? O_RSYNC    : 0) |
            ((fdflags & WASI_FDFLAG_SYNC    ) ? O_SYNC     : 0));
    if ((fs_rights_base & WASI_RIGHT_FD_WRITE) &&
        (fs_rights_base & WASI_RIGHT_FD_READ)) {
        flags |= O_RDWR;
    } else if (fs_rights_base & WASI_RIGHT_FD_WRITE) {
        flags |= O_WRONLY;
    } else if (fs_rights_base & WASI_RIGHT_FD_READ) {
        flags |= O_RDONLY;
    }

    int mode = 0644;
    int fd = openat(dirfd, path, flags, mode);
    if (fd < 0) {
        return wasi_fromerrno(errno);
    }

    set_i32(fd_off, fd);
    return WASI_ESUCCESS;
}

/**
 * @brief Used by a WASI module to copy the argument buffer into linear memory write an 
 * indirect to the base offset of the buffer 
 * 
 * @param argv_p_offset 
 * @param argv_buf_offset 
 * @return i32 
 */
i32 wasi_snapshot_preview1_args_get(u32 argv_p_offset, u32 argv_buf_offset){
    char *argv_buf = get_memory_ptr_for_runtime(argv_buf_offset, runtime_argv_buffer_len);
    memcpy(argv_buf, runtime_argv_buffer, runtime_argv_buffer_len);

    // Write the vector of argument base offset
    u32 *argv_p = (u32 *)get_memory_ptr_for_runtime(argv_p_offset, sizeof(u32) * runtime_argc);
    for (u32 i = 0; i < runtime_argc; i++){
        *(argv_p++) = argv_buf_offset + runtime_argv_buffer_offsets[i];
    }

    // TODO: What is the expected return type here
    return 0;
}

/**
 * @brief Used by a WASI module to determine the argument count and size of the requried
 * argument buffer
 * 
 * @param argc linear memory offset where we should write argc
 * @param args_size linear memory offset where we should write the length of the args buffer
 * @return i32 
 */
i32 wasi_snapshot_preview1_args_sizes_get(i32 argc_offset, i32 args_size_offset){
    u32 *argc = (u32 *)get_memory_ptr_for_runtime(argc_offset, sizeof(i32));
    *argc = runtime_argc;
    
    u32 *args_size = (u32 *)get_memory_ptr_for_runtime(args_size_offset, sizeof(i32));
    *args_size = runtime_argv_buffer_len;

    // TODO: What is the expected return type here?
    return 0;
}

i32 wasi_snapshot_preview1_fd_close(i32 fd) {
    i32 res = (i32) close(fd);

    if (res == -1) {
        return wasi_fromerrno(errno);
    }
    return res;
}

i32 wasi_snapshot_preview1_fd_fdstat_get(i32 fd, u32 buf_offset) {
    struct wasi_fdstat* fdstat = get_memory_ptr_void(buf_offset, sizeof(struct wasi_fdstat));

    struct stat stat;
    i32 res = fstat(fd, &stat);
    if (res == -1) {
        return wasi_fromerrno(errno);
    }
    int mode = stat.st_mode;

    i32 fl = fcntl(fd, F_GETFL);
    if (fl < 0) {
        return wasi_fromerrno(errno);
    }

    fdstat->fs_filetype = (
            (S_ISBLK(mode)  ? WASI_FILETYPE_BLOCK_DEVICE     : 0) |
            (S_ISCHR(mode)  ? WASI_FILETYPE_CHARACTER_DEVICE : 0) |
            (S_ISDIR(mode)  ? WASI_FILETYPE_DIRECTORY        : 0) |
            (S_ISREG(mode)  ? WASI_FILETYPE_REGULAR_FILE     : 0) |
            (S_ISSOCK(mode) ? WASI_FILETYPE_SOCKET_STREAM    : 0) |
            (S_ISLNK(mode)  ? WASI_FILETYPE_SYMBOLIC_LINK    : 0));
    fdstat->fs_flags = (
            ((fl & O_APPEND)   ? WASI_FDFLAG_APPEND     : 0) |
            ((fl & O_DSYNC)    ? WASI_FDFLAG_DSYNC      : 0) |
            ((fl & O_NONBLOCK) ? WASI_FDFLAG_NONBLOCK   : 0) |
            ((fl & O_RSYNC)    ? WASI_FDFLAG_RSYNC      : 0) |
            ((fl & O_SYNC)     ? WASI_FDFLAG_SYNC       : 0));
    fdstat->fs_rights_base = 0; // all rights
    fdstat->fs_rights_inheriting = 0; // all rights

    return WASI_ESUCCESS;
}

i32 wasi_snapshot_preview1_fd_fdstat_set_flags(i32 fd, u32 fdflags) {
    int flags = (
        ((flags & WASI_FDFLAG_APPEND  ) ? O_APPEND   : 0) |
        ((flags & WASI_FDFLAG_DSYNC   ) ? O_DSYNC    : 0) |
        ((flags & WASI_FDFLAG_NONBLOCK) ? O_NONBLOCK : 0) |
        ((flags & WASI_FDFLAG_RSYNC   ) ? O_RSYNC    : 0) |
        ((flags & WASI_FDFLAG_SYNC    ) ? O_SYNC     : 0));
    int err = fcntl(fd, F_SETFL, fdflags);
    if (err < 0) {
        return wasi_fromerrno(errno);
    }
    return WASI_ESUCCESS;
}

i32 wasi_snapshot_preview1_fd_seek(i32 fd, i64 file_offset, i32 whence, u32 newoffset_off) {
    off_t res = lseek(fd, (off_t)file_offset, whence);

    if (res == -1) {
        return wasi_fromerrno(errno);
    }

    set_i64(newoffset_off, res);
    return WASI_ESUCCESS;
}

i32 wasi_snapshot_preview1_fd_datasync(i32 fd) {
    int res = fdatasync(fd);
    if (res == -1) {
        return wasi_fromerrno(errno);
    }

    return WASI_ESUCCESS;
}

i32 wasi_snapshot_preview1_fd_read(i32 fd, i32 iov_offset, i32 iovcnt, i32 nread_off) {
    i32 sum = 0;
    struct wasi_iovec *iov = get_memory_ptr_void(iov_offset, iovcnt * sizeof(struct wasi_iovec));

    for (int i = 0; i < iovcnt; i++) {
        void* ptr = get_memory_ptr_void(iov[i].base_offset, iov[i].len);
        ssize_t res = read(fd, ptr, iov[i].len);
        if (res == -1) {
            return wasi_fromerrno(errno);
        }

        sum += res;
    }

    set_i32(nread_off, sum);
    return WASI_ESUCCESS;
}

i32 wasi_snapshot_preview1_fd_write(i32 fd, i32 iov_offset, i32 iovcnt, i32 nwritten_off) {
    i32 sum = 0;
    struct wasi_iovec *iov = get_memory_ptr_void(iov_offset, iovcnt * sizeof(struct wasi_iovec));

    for (int i = 0; i < iovcnt; i++) {
        void* ptr = get_memory_ptr_void(iov[i].base_offset, iov[i].len);
        ssize_t res = write(fd, ptr, iov[i].len);
        if (res == -1) {
            return wasi_fromerrno(errno);
        }

        sum += res;
    }

    set_i32(nwritten_off, sum);
    return WASI_ESUCCESS;
}

// other filesystem operations
i32 wasi_snapshot_preview1_path_filestat_get(i32 fd, u32 flags, u32 path_off, u32 path_len, u32 buf_off) {
    // get path/filestat
    char* path = get_memory_string(path_off);
    struct wasi_filestat* filestat = get_memory_ptr_void(buf_off, sizeof(struct wasi_filestat));

    struct stat stat;
    int res = fstatat(fd, path, &stat, 0);
    if (res == -1) {
        return wasi_fromerrno(errno);
    }

    filestat->st_dev = stat.st_dev;
    filestat->st_ino = stat.st_ino;
    filestat->st_filetype =  stat.st_mode;
    filestat->st_nlink = stat.st_nlink;
    filestat->st_size = stat.st_size;
    filestat->st_atim = stat.st_atime;
    filestat->st_mtim = stat.st_mtime;
    filestat->st_ctim = stat.st_ctime;

    return WASI_ESUCCESS;
}

i32 wasi_snapshot_preview1_path_unlink_file(i32 fd, u32 path_off, u32 path_len) {
    // get path
    char* path = get_memory_string(path_off);

    int res = unlinkat(fd, path, 0);
    if (res == -1) {
        return wasi_fromerrno(errno);
    }

    return WASI_ESUCCESS;
}

i32 wasi_snapshot_preview1_path_create_directory(i32 fd, u32 path_off, u32 path_len) {
    // get path
    char* path = get_memory_string(path_off);

    int res = mkdirat(fd, path, 0777);
    if (res == -1) {
        return wasi_fromerrno(errno);
    }

    return WASI_ESUCCESS;
}

// clock operations
i32 wasi_snapshot_preview1_clock_time_get(u32 clock_id, u64 precision, u32 time_off) {
    struct timespec tp;
    clock_gettime(clock_id, &tp);
    set_i64(time_off, (uint64_t)tp.tv_sec*1000000000ULL + (uint64_t)tp.tv_nsec);
    return WASI_ESUCCESS;
}

// process operations
__attribute__((noreturn))
void wasi_snapshot_preview1_proc_exit(i32 exitcode) {
    exit(exitcode);
}

// Unsupported WASI calls
// TODO: wasi_snapshot_preview1_environ_get
// TODO: wasi_snapshot_preview1_environ_sizes_get
// TODO: wasi_snapshot_preview1_clock_res_get
// TODO: wasi_snapshot_preview1_fd_advise
// TODO: wasi_snapshot_preview1_fd_allocate
// TODO: wasi_snapshot_preview1_fd_fdstat_set_rights
// TODO: wasi_snapshot_preview1_fd_filestat_get
// TODO: wasi_snapshot_preview1_fd_filestat_set_size
// TODO: wasi_snapshot_preview1_fd_filestat_set_times
// TODO: wasi_snapshot_preview1_fd_pread
// TODO: wasi_snapshot_preview1_fd_prestat_get
// TODO: wasi_snapshot_preview1_fd_prestat_dir_name
// TODO: wasi_snapshot_preview1_fd_pwrite
// TODO: wasi_snapshot_preview1_fd_readdir
// TODO: wasi_snapshot_preview1_fd_renumber
// TODO: wasi_snapshot_preview1_fd_sync
// TODO: wasi_snapshot_preview1_fd_tell
// TODO: wasi_snapshot_preview1_path_filestat_set_times
// TODO: wasi_snapshot_preview1_path_link
// TODO: wasi_snapshot_preview1_path_readlink
// TODO: wasi_snapshot_preview1_path_remove_directory
// TODO: wasi_snapshot_preview1_path_rename
// TODO: wasi_snapshot_preview1_path_symlink
// TODO: wasi_snapshot_preview1_poll_oneoff
// TODO: wasi_snapshot_preview1_proc_raise
// TODO: wasi_snapshot_preview1_sched_yield
// TODO: wasi_snapshot_preview1_random_get
// TODO: wasi_snapshot_preview1_sock_recv
// TODO: wasi_snapshot_preview1_sock_send
// TODO: wasi_snapshot_preview1_sock_shutdown