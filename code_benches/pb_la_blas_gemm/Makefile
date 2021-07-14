bin/pb_la_blas_gemm.wasm: ../dummy.c gemm.c polybench.c
	../../wasi-sdk/bin/clang -Wl,--allow-undefined,-z,stack-size=32768,--threads=1 --target=wasm32-wasi -mcpu=mvp -O3 -flto --sysroot=../../wasi-sdk/share/wasi-sysroot  -O3 -flto ../dummy.c gemm.c polybench.c -o bin/pb_la_blas_gemm.wasm

bin/pb_la_blas_gemm.bc: bin/pb_la_blas_gemm.wasm
	../../target/release/awsm  bin/pb_la_blas_gemm.wasm -o bin/pb_la_blas_gemm.bc

bin/pb_la_blas_gemm: gemm.c polybench.c
	clang -lm  -O3 -flto -g gemm.c polybench.c -o bin/pb_la_blas_gemm

bin/pb_la_blas_gemm_cm: bin/pb_la_blas_gemm.bc ../../runtime/runtime.c ../../runtime/libc/wasi_sdk_backing.c ../../runtime/libc/env.c ../../runtime/memory/cortex_m.c
	clang -lm  -O3 -flto -g bin/pb_la_blas_gemm.bc ../../runtime/runtime.c ../../runtime/libc/wasi_sdk_backing.c ../../runtime/libc/env.c ../../runtime/memory/cortex_m.c -o bin/pb_la_blas_gemm_cm

bin/pb_la_blas_gemm_np_us: bin/pb_la_blas_gemm_us.bc ../../runtime/runtime.c ../../runtime/libc/wasi_sdk_backing.c ../../runtime/libc/env.c ../../runtime/memory/no_protection.c
	clang -lm  -O3 -flto -g bin/pb_la_blas_gemm_us.bc ../../runtime/runtime.c ../../runtime/libc/wasi_sdk_backing.c ../../runtime/libc/env.c ../../runtime/memory/no_protection.c -o bin/pb_la_blas_gemm_np_us

bin/pb_la_blas_gemm_np: bin/pb_la_blas_gemm.bc ../../runtime/runtime.c ../../runtime/libc/wasi_sdk_backing.c ../../runtime/libc/env.c ../../runtime/memory/no_protection.c
	clang -lm  -O3 -flto -g bin/pb_la_blas_gemm.bc ../../runtime/runtime.c ../../runtime/libc/wasi_sdk_backing.c ../../runtime/libc/env.c ../../runtime/memory/no_protection.c -o bin/pb_la_blas_gemm_np

bin/pb_la_blas_gemm_bc: bin/pb_la_blas_gemm.bc ../../runtime/runtime.c ../../runtime/libc/wasi_sdk_backing.c ../../runtime/libc/env.c ../../runtime/memory/generic.c
	clang -lm  -O3 -flto -g bin/pb_la_blas_gemm.bc ../../runtime/runtime.c ../../runtime/libc/wasi_sdk_backing.c ../../runtime/libc/env.c ../../runtime/memory/generic.c -o bin/pb_la_blas_gemm_bc

bin/pb_la_blas_gemm_vm: bin/pb_la_blas_gemm.bc ../../runtime/runtime.c ../../runtime/libc/wasi_sdk_backing.c ../../runtime/libc/env.c ../../runtime/memory/64bit_nix.c
	clang -lm  -O3 -flto -g bin/pb_la_blas_gemm.bc ../../runtime/runtime.c ../../runtime/libc/wasi_sdk_backing.c ../../runtime/libc/env.c ../../runtime/memory/64bit_nix.c -o bin/pb_la_blas_gemm_vm