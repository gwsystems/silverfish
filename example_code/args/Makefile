.PHONY: clean
clean:
	rm -f args.wasm args.bc args_vm

args.wasm: args.c
	../../wasi-sdk/bin/clang -Wl,--allow-undefined,-z,stack-size=32768,--threads=1 --target=wasm32-wasi -mcpu=mvp -O3 -flto --sysroot=../../wasi-sdk/share/wasi-sysroot  -O3 -flto args.c -o args.wasm

args.bc: args.wasm
	../../target/release/awsm  args.wasm -o args.bc

args_vm: args.bc ../../runtime/runtime.c ../../runtime/libc/wasi_sdk_backing.c ../../runtime/libc/env.c ../../runtime/memory/64bit_nix.c
	clang -lm  -O3 -flto -g args.bc ../../runtime/runtime.c ../../runtime/libc/wasi_sdk_backing.c ../../runtime/libc/env.c ../../runtime/memory/64bit_nix.c -o args_vm