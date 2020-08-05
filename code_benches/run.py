#!/usr/bin/env python3

from itertools import chain
import csv
import glob
import os
import subprocess as sp
import sys
import timeit

# Note: This is a major configuration option, you probably want to set this if you're doing anything non-trivial
SILVERFISH_TARGET = None
# SILVERFISH_TARGET = "thumbv7em-none-unknown-eabi"
# SILVERFISH_TARGET = "x86_64-apple-macosx10.15.0"
# SILVERFISH_TARGET = "x86_64-pc-linux-gnu"

# CSV file name
CSV_NAME = "benchmarks.csv"

# Make sure we're in the code_benches directory
if os.path.dirname(sys.argv[0]):
    os.chdir(os.path.dirname(sys.argv[0]))
# Absolute path to the `code_benches` directory
BENCH_ROOT = os.getcwd()
# Absolute path to the `silverfish` directory
ROOT_PATH = os.path.dirname(BENCH_ROOT)

RUNTIME_PATH = ROOT_PATH + "/runtime"

SILVERFISH_RELEASE_PATH = ROOT_PATH + "/target/release/silverfish"
SILVERFISH_DEBUG_PATH = ROOT_PATH + "/target/debug/silverfish"
assert all(arg in {"--release", "--debug"} for arg in sys.argv[1:])
if "--release" in sys.argv:
    SILVERFISH_PATH = SILVERFISH_RELEASE_PATH
elif "--debug" in sys.argv:
    SILVERFISH_PATH = SILVERFISH_DEBUG_PATH
else:
    def getmtime_or_zero(path):
        try:
            return os.path.getmtime(path)
        except FileNotFoundError:
            return 0
    SILVERFISH_PATH = max(
        [SILVERFISH_RELEASE_PATH, SILVERFISH_DEBUG_PATH],
        key=getmtime_or_zero)

WASMCEPTION_PATH = ROOT_PATH + "/wasmception"

# Our special WASM clang is under this wasmception path
WASM_CLANG = WASMCEPTION_PATH + "/dist/bin/clang"
# These flags are all somewhat important -- see @Others for more information
WASM_LINKER_FLAGS = "-Wl,--allow-undefined,-z,stack-size={stack_size},--no-threads,--stack-first,--no-entry,--export-all,--export=main,--export=dummy"
# Point WASM to our custom libc
WASM_SYSROOT_FLAGS = "--sysroot={}/sysroot".format(WASMCEPTION_PATH)
WASM_FLAGS = WASM_LINKER_FLAGS + " --target=wasm32-unknown-unknown-wasm -nostartfiles -O3 -flto " + WASM_SYSROOT_FLAGS

# What is the machine we're running on like?
IS_64_BIT = sys.maxsize > 2**32
IS_X86 = '86' in os.uname()[-1]
IS_32_BIT_X86 = (not IS_64_BIT) and IS_X86

# How many times should we run our benchmarks
RUN_COUNT = 10
ENABLE_DEBUG_SYMBOLS = True

COMPILE_WASM_ONLY = False
if SILVERFISH_TARGET == "thumbv7em-none-unknown-eabi" and not COMPILE_WASM_ONLY:
    print("run.py: thumbv7em-none-unknown-eabi wasm->bc is target specific, refusing to compile native code!")
    COMPILE_WASM_ONLY = True


# FIXME: Mibench runs many of these programs multiple times, which is probably worth replicating
class Program(object):
    def __init__(self, name, parameters, stack_size, custom_arguments=None, do_lto=True, is_cpp=False):
        self.name = name
        self.parameters = parameters
        self.stack_size = stack_size
        self.custom_arguments = ""
        if custom_arguments:
            self.custom_arguments = " ".join(custom_arguments)
        self.do_lto = do_lto
        self.is_cpp = is_cpp

    def __str__(self):
        return "{}({})".format(self.name, " ".join(map(str, self.parameters)))

    def sources(self):
        # glob here, avoids issues with glob expansion in shell
        if self.is_cpp:
            patterns = ["*.c", "*.cpp"]
        else:
            patterns = ["*.c"]

        paths = (os.path.join(self.name, pattern) for pattern in patterns)
        sources = chain.from_iterable(glob.glob(path) for path in paths)
        return " ".join(source[len(self.name)+1:] for source in sources)


# These are the programs we're going to test with
# TODO: Fix ispell, which doesn't compile on OS X
# TODO: Fix sphinx, which doesn't work properly on OS X
# TODO: Do ghostscript, which has a ton of files
programs = [
    # Program("_test", [], 2**15),
    # == Custom Benchmarks ==
    Program("custom_binarytrees", [16], 2 ** 14),
    Program("custom_function_pointers", [], 2 ** 14),
    Program("custom_libjpeg", [], 2 ** 15,
            custom_arguments=["-Wno-incompatible-library-redeclaration", "-Wno-implicit-function-declaration",
                              "-Wno-shift-negative-value"]),
    Program("custom_matrix_multiply", [], 2 ** 14),
    Program("custom_memcmp", [], 2 ** 14),
    Program("custom_sqlite", [], 2 ** 15, custom_arguments=["-DSQLITE_MUTEX_NOOP", "-ldl"]),

    # == Apps ==
    # Program("app_nn", [], 2 ** 14, custom_arguments=["-std=c99", "-Wno-unknown-attributes", "-DARM_MATH_CM3", "-I/Users/peachg/Projects/CMSIS_5_NN/CMSIS_5/CMSIS/DSP/Include", "-I/Users/peachg/Projects/CMSIS_5_NN/CMSIS_5/CMSIS/Core/Include", "-I/Users/peachg/Projects/CMSIS_5_NN/CMSIS_5/CMSIS/NN/Include"]),
#    Program("app_pid", ["-std=c++11", "-Wall"], 2 ** 8, custom_arguments=[], is_cpp=True),
#    Program("app_tiny_ekf", ["-std=c++11", "-Wall"], 2 ** 14, custom_arguments=[], is_cpp=True),
#    Program("app_tinycrypt", [], 2 ** 15 + 2**14, custom_arguments=[ "-Wall", "-Wpedantic", "-Wno-gnu-zero-variadic-macro-arguments", "-std=c11", "-I/Users/peachg/Projects/silverfish/code_benches/app_tinycrypt/", "-DENABLE_TESTS", "-I."]),
    # Program("app_v9", [], 2 ** 18, custom_arguments=[], do_lto=False),

    # == MiBench ==
    # TODO: Make non file version
    # Program("mi_adpcm", ["< large.pcm"], 2 ** 14,
    #         custom_arguments=["-Wno-implicit-int", "-Wno-implicit-function-declaration"]),
    # Program("mi_basic_math", [], 2 ** 14),
    # Program("mi_bitcount", [2 ** 24], 2 ** 14),
    # Program("mi_bitcount_cm", [], 2 ** 14),
    # # TODO: Make non file version
    # Program("mi_crc", ["large.pcm"], 2 ** 14, custom_arguments=["-Wno-implicit-int", "-Wno-format"]),
    # Program("mi_dijkstra", ["input.dat"], 2 ** 14,
    #         custom_arguments=["-Wno-return-type"]),
    # Program("mi_dijkstra_cm", [], 2 ** 14,
    #         custom_arguments=["-Wno-return-type"]),
    # Program("mi_fft", [8, 32768], 2 ** 14),
    # Program("mi_fft_cm", [], 2 ** 15),
    # Program("mi_gsm", ["-fps", "-c", "large.au"], 2 ** 15, custom_arguments=["-DSASR", "-Wno-everything"]),
    # Program("mi_mandelbrot", [5000], 2 ** 14),
    # Program("mi_mandelbrot_cm", [], 2 ** 14),
    # TODO: Make non file version
    # Program("mi_patricia", ["large.udp"], 2 ** 14),
    # Program("mi_qsort", ["input_small.dat"], 2 ** 18),
    # Program("mi_qsort_cm", [""], 2 ** 18),
    # Program("mi_rsynth", ["-a", "-q", "-o", "/dev/null", "< largeinput.txt"], 2**14,
    #         custom_arguments=["-I.", "-Wno-everything", "-I/usr/local/include/"]),
    # # TODO: Make non file version
    # Program("mi_sha", ["input_large.asc"], 2 ** 14),
    # Program("mi_susan", ["input_large.pgm", "/dev/null", "-s"], 2 ** 19, custom_arguments=["-Wno-everything"]),
    # Program("mi_stringsearch", [], 2 ** 13),
    # TODO: These programs segfault on my computer...
    # Program("mi_blowfish", ["e", "input_large.asc", "/dev/null", "1234567890abcdeffedcba0987654321"], 2**14),
    # Program("mi_pgp", ['-sa -z "this is a test" -u taustin@eecs.umich.edu testin.txt austin@umich.edu'], 2 ** 14,
    #         custom_arguments=["-DUNIX -D_BSD -DPORTABLE -DUSE_NBIO -DMPORTABLE", "-I.", "-Wno-everything"]),

    # == Polybench ==
    Program("pb_datamining_correlation", [], 2**15),
    Program("pb_datamining_covariance", [], 2**15),

    Program("pb_la_blas_gemm", [], 2**15),
    Program("pb_la_blas_gemver", [], 2**15),
    Program("pb_la_blas_gesummv", [], 2**15),
    Program("pb_la_blas_symm", [], 2**15),
    Program("pb_la_blas_syr2k", [], 2**15),
    Program("pb_la_blas_syrk", [], 2**15),
    Program("pb_la_blas_trmm", [], 2**15),

    Program("pb_la_kernels_2mm", [], 2**15),
    Program("pb_la_kernels_3mm", [], 2**15),
    Program("pb_la_kernels_atax", [], 2**15),
    Program("pb_la_kernels_bicg", [], 2**15),
    Program("pb_la_kernels_doitgen", [], 2**15),
    Program("pb_la_kernels_mvt", [], 2**15),

    Program("pb_la_solvers_cholesky", [], 2**15),
    Program("pb_la_solvers_durbin", [], 2**15),
    Program("pb_la_solvers_gramschmidt", [], 2**15),
    Program("pb_la_solvers_lu", [], 2**15),
    Program("pb_la_solvers_ludcmp", [], 2**15),
    Program("pb_la_solvers_trisolv", [], 2**15),

    Program("pb_medely_deriche", [], 2**15),
    Program("pb_medely_floyd_warshall", [], 2**15),
    Program("pb_medely_nussinov", [], 2**15),

    Program("pb_stencils_adi", [], 2**15),
    Program("pb_stencils_fdtd_2d", [], 2**15),
    Program("pb_stencils_heat_3d", [], 2**15),
    Program("pb_stencils_jacobi_1d", [], 2**15),
    Program("pb_stencils_jacobi_2d", [], 2**15),
    Program("pb_stencils_seidel_2d", [], 2**15),
]


# Compile the C code in `program`'s directory into a native executable
def compile_to_executable(program):
    opt = "-O3"
    if program.do_lto:
        opt += " -flto"
    if ENABLE_DEBUG_SYMBOLS:
        opt += " -g"
    if program.is_cpp:
        clang = "clang++"
    else:
        clang = "clang"

    command = "{clang} {args} -lm {opt} {sources} -o bin/{pname}" \
        .format(clang=clang, args=program.custom_arguments, opt=opt, sources=program.sources(), pname=program.name)
    print(command)
    sp.check_call(command, shell=True, cwd=program.name)

# Compile the C code in `program`'s directory into WASM
def compile_to_wasm(program):
    flags = WASM_FLAGS.format(stack_size=program.stack_size)
    command = "{clang} {flags} {args} -O3 -flto ../dummy.c {sources} -o bin/{pname}.wasm" \
        .format(clang=WASM_CLANG, flags=flags, sources=program.sources(), args=program.custom_arguments, pname=program.name)
    print(command)
    sp.check_call(command, shell=True, cwd=program.name)


# Compile the WASM in `program`'s directory into llvm bytecode
def compile_wasm_to_bc(program):
    if SILVERFISH_TARGET is None:
        target_flag = ""
    else:
        target_flag = "--target " + SILVERFISH_TARGET

    command = "{silverfish} {target} bin/{pname}.wasm -o bin/{pname}.bc"\
        .format(silverfish=SILVERFISH_PATH, target=target_flag, pname=program.name)
    print(command)
    sp.check_call(command, shell=True, cwd=program.name)
    # Also compile an unsafe version, so we can see the performance difference
    command = "{silverfish} {target} -u bin/{pname}.wasm -o bin/{pname}_us.bc"\
        .format(silverfish=SILVERFISH_PATH, target=target_flag, pname=program.name)
    print(command)
    sp.check_call(command, shell=True, cwd=program.name)


# Compile the WASM in `program`'s directory into llvm bytecode
def compile_wasm_to_executable(program, exe_postfix, memory_impl, unsafe_impls=False):
    bc_file = "bin/{pname}.bc".format(pname=program.name)
    if unsafe_impls:
        bc_file = "bin/{pname}_us.bc".format(pname=program.name)
    opt = "-O3"
    if program.do_lto:
        opt += " -flto"
    if ENABLE_DEBUG_SYMBOLS:
        opt += " -g"

    if SILVERFISH_TARGET is None:
        target_flag = ""
    else:
        target_flag = "-target " + SILVERFISH_TARGET

    command = "clang -lm {target} {opt} {bc_file} {runtime}/runtime.c {runtime}/libc/libc_backing.c {runtime}/libc/env.c {runtime}/memory/{mem_impl} -o bin/{pname}_{postfix}"\
        .format(target=target_flag, opt=opt, bc_file=bc_file, pname=program.name, runtime=RUNTIME_PATH, mem_impl=memory_impl, postfix=exe_postfix)
    print(command)
    sp.check_call(command, shell=True, cwd=program.name)


# Execute executable `p` with arguments `args` in directory `dir`
def execute(p, args, dir):
    command = p + " " + args
    sp.check_call(command, shell=True, stdout=sp.DEVNULL, stderr=sp.DEVNULL, cwd=dir)


# Benchmark the given program's executable
#   p = the program
#   exe_postfix = what postfix we gave the executable in `compile_wasm_to_executable`
#   name = the human readable name for this version of the executable
def bench(p, exe_postfix, name):
    print("Testing {} {}".format(p, name))
    path = "{broot}/{pname}/bin/{pname}{pf}".format(broot=BENCH_ROOT, pname=p.name, pf=exe_postfix)
    command = "execute('{path}', '{args}', '{dir}')".format(path=path, args=' '.join(map(str, p.parameters)), dir=p.name)
    return min(timeit.repeat(command, 'from __main__ import execute', number=1, repeat=RUN_COUNT))


# Output a run's execution time, telling us how much faster or slower it is
def output_run(base_time, execution_time):
    base_time = round(base_time, 4)
    execution_time = round(execution_time, 4)
    if execution_time > base_time:
        print("{:.4f} ({:.2f}% slower)".format(execution_time, ((execution_time - base_time) / base_time) * 100))
    else:
        print("{:.4f} ({:.2f}% faster)".format(execution_time, ((base_time - execution_time) / base_time) * 100))


if __name__ == "__main__":
    # FIXME: Old code for calculating the heap ptr
    # for p in programs:
    #     path = "{broot}/{pname}/bin/{pname}.wasm".format(broot=BENCH_ROOT, pname=p.name)
    #     print(p.name)
    #     sp.call("wasm2wat {} | grep 'global (;1;) i32'".format(path), shell=True)


    # Compile all our programs
    for i, p in enumerate(programs):
        print("Compiling {} {}/{}".format(p.name, i + 1, len(programs)))

        os.makedirs(p.name + "/bin", exist_ok=True)
        compile_to_wasm(p)
        compile_wasm_to_bc(p)
        if COMPILE_WASM_ONLY:
            continue

        compile_to_executable(p)

        compile_wasm_to_executable(p, "cm", "cortex_m.c")
        compile_wasm_to_executable(p, "np_us", "no_protection.c", True)
        compile_wasm_to_executable(p, "np", "no_protection.c")
        compile_wasm_to_executable(p, "bc", "generic.c")
        if IS_64_BIT:
            compile_wasm_to_executable(p, "vm", "64bit_nix.c")
        if IS_X86:
            compile_wasm_to_executable(p, "mpx", "mpx.c")
        if IS_32_BIT_X86:
            compile_wasm_to_executable(p, "sm", "segmented.c")

    if COMPILE_WASM_ONLY:
        sys.exit(0)

    print()
    print("Outputting to " + CSV_NAME)
    print()

    with open(CSV_NAME, 'w+', newline='') as csv_file:
        csv_writer = csv.writer(csv_file)

        columns = ["Program", "native", "wasm no protection unsafe", "wasm no protection", "wasm bounds checked"]
        if IS_X86:
            columns.append("wasm mpx")
        if IS_64_BIT:
            columns.append("wasm virtual memory")
        if IS_32_BIT_X86:
            columns.append("wasm segmentation")
        csv_writer.writerow(columns)

        # Benchmark and output timing info for each of our programs
        for p in programs:
            csv_row = [p.name]

            base_speed = bench(p, "", "native")
            print("{:.4f}".format(base_speed))
            csv_row.append(base_speed / base_speed)

            np_u_speed = bench(p, "_np_us", "wasm no protection unsafe")
            output_run(base_speed, np_u_speed)
            csv_row.append(np_u_speed / base_speed)

            np_speed = bench(p, "_np", "wasm no protection")
            output_run(base_speed, np_speed)
            csv_row.append(np_speed / base_speed)

            bc_speed = bench(p, "_bc", "wasm bounds checked")
            output_run(base_speed, bc_speed)
            csv_row.append(bc_speed / base_speed)

            if IS_X86:
                mpx_speed = bench(p, "_mpx", "wasm mpx")
                output_run(base_speed, mpx_speed)
                csv_row.append(mpx_speed / base_speed)
            if IS_64_BIT:
                vm_speed = bench(p, "_vm", "wasm virtual memory")
                output_run(base_speed, vm_speed)
                csv_row.append(vm_speed / base_speed)
            if IS_32_BIT_X86:
                sm_speed = bench(p, "_sm", "wasm using segmentation")
                output_run(base_speed, sm_speed)
                csv_row.append(sm_speed / base_speed)
            csv_writer.writerow(csv_row)
            print("")
