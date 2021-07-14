CC=clang
OPTFLAGS=-O3 -flto

# Assuming to be called from ./tests/wat directory
ROOT_PATH:=$(shell cd ../.. && realpath .)
RUNTIME_PATH:=${ROOT_PATH}/runtime

AWSM_CC:=${ROOT_PATH}/target/release/awsm

WASI_SDK_PATH:=${ROOT_PATH}/wasi-sdk
WASI_SDK_CLANG:=${WASI_SDK_PATH}/bin/clang
WASI_SDK_SYSROOT:=${WASI_SDK_PATH}/share/wasi-sysroot
WASI_SDK_FLAGS:=--target=wasm32-wasi -mcpu=mvp -nostartfiles ${OPTFLAGS} --sysroot=${WASI_SDK_SYSROOT}
WASI_SDK_BACKING:=wasi_sdk_backing.c

WASM_CC:=${WASI_SDK_CLANG}
WASM_FLAGS:=${WASI_SDK_FLAGS}
WASM_BACKING:=${WASI_SDK_BACKING}

# Excludes stack-size, as this may vary between apps
WASM_LINKER_FLAGS:=-Wl,--allow-undefined,--no-threads,--stack-first,--no-entry,--export-all,--export=main,--export=dummy

clean: if_clean args_clean

# Should work for Linux. Alternate installation options at https://wasmtime.dev/
.PHONY: install_wasmtime
install_wasmtime:
	@curl https://wasmtime.dev/install.sh -sSf | bash

.PHONY: install_wabt
install_wabt:
	@./install_wabt.sh

if.wasm:
	@wat2wasm if.wat -o if.wasm

if.bc: if.wasm
	@RUST_BACKTRACE=1 ${AWSM_CC} ./if.wasm -o ./if.bc

.PHONY: if_wasmtime
if_wasmtime: if.wasm
	@wasmtime if.wasm

if_vm: if.bc
	@${CC} -lm ${OPTFLAGS} \
	./if.bc \
	${RUNTIME_PATH}/runtime.c \
	${RUNTIME_PATH}/libc/${WASM_BACKING} \
	${RUNTIME_PATH}/libc/env.c \
	${RUNTIME_PATH}/memory/64bit_nix.c \
	-o ./if_vm

.PHONY: if_clean
if_clean:
	@rm -f if.wasm if.bc if_vm

args.wasm: args.wat
	@wat2wasm args.wat -o args.wasm

.PHONY: args_wasmtime
args_wasmtime: args.wasm
	@wasmtime args.wasm arg1 arg2 arg3

args.bc: args.wasm
	@RUST_BACKTRACE=1 ${AWSM_CC} ./args.wasm -o ./args.bc

args_vm: args.bc
	@${CC} -lm ${OPTFLAGS} \
	./args.bc \
	${RUNTIME_PATH}/runtime.c \
	${RUNTIME_PATH}/libc/${WASM_BACKING} \
	${RUNTIME_PATH}/libc/env.c \
	${RUNTIME_PATH}/memory/64bit_nix.c \
	-o ./args_vm

.PHONY: args_clean
args_clean:
	@rm -f args.wasm args.bc args_vm