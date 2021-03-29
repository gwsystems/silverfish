use llvm::Builder;
use llvm::Compile;
use llvm::Context as LLVMCtx;
use llvm::FunctionType;
use llvm::Module as LLVMModule;
use llvm::Sub;
use llvm::Value;

use wasmparser::Type;

use crate::wasm::Global;
use crate::wasm::Instruction;
use crate::Opt;

use crate::codegen::runtime_stubs::*;
use crate::codegen::type_conversions::llvm_type_to_wasm_type;
use crate::codegen::type_conversions::wasm_type_to_llvm_type;
use crate::codegen::ModuleCtx;

pub enum GlobalValue<'a> {
    InlinedConstant(&'a Value),
    Native(&'a Value),
    RuntimeI32(u32),
    RuntimeI64(u32),
}

impl<'a> GlobalValue<'a> {
    pub fn load(&self, m_ctx: &'a ModuleCtx, b: &'a Builder) -> &'a Value {
        match self {
            GlobalValue::InlinedConstant(v) => v,
            GlobalValue::Native(ptr) => b.build_load(ptr),
            GlobalValue::RuntimeI32(idx) => {
                let func = get_stub_function(m_ctx, GET_GLOBAL_I32);
                b.build_call(func, &[idx.compile(m_ctx.llvm_ctx)])
            }
            GlobalValue::RuntimeI64(idx) => {
                let func = get_stub_function(m_ctx, GET_GLOBAL_I64);
                b.build_call(func, &[idx.compile(m_ctx.llvm_ctx)])
            }
        }
    }

    pub fn store(&self, m_ctx: &'a ModuleCtx, b: &'a Builder, v: &'a Value) {
        match self {
            GlobalValue::InlinedConstant(_) => panic!("Cannot write to an inlined constant"),
            GlobalValue::Native(ptr) => {
                b.build_store(v, ptr);
            }
            GlobalValue::RuntimeI32(idx) => {
                let func = get_stub_function(m_ctx, SET_GLOBAL_I32);
                b.build_call(func, &[idx.compile(m_ctx.llvm_ctx), v]);
            }
            GlobalValue::RuntimeI64(idx) => {
                let func = get_stub_function(m_ctx, SET_GLOBAL_I64);
                b.build_call(func, &[idx.compile(m_ctx.llvm_ctx), v]);
            }
        }
    }
}

pub fn insert_globals<'a>(
    opt: &Opt,
    llvm_ctx: &'a LLVMCtx,
    llvm_module: &'a LLVMModule,
    globals: Vec<Global>,
) -> Vec<GlobalValue<'a>> {
    if opt.use_runtime_global_handling {
        insert_runtime_globals(opt, llvm_ctx, llvm_module, globals)
    } else {
        insert_native_globals(opt, llvm_ctx, llvm_module, globals)
    }
}

fn insert_native_globals<'a>(
    opt: &Opt,
    llvm_ctx: &'a LLVMCtx,
    llvm_module: &'a LLVMModule,
    globals: Vec<Global>,
) -> Vec<GlobalValue<'a>> {
    let mut global_values = Vec::new();
    for g in globals {
        let v = match g {
            Global::Imported {
                name,
                content_type,
                mutable,
            } => {
                let llvm_global =
                    llvm_module.add_global(&name, wasm_type_to_llvm_type(&*llvm_ctx, content_type));
                llvm_global.set_constant(!mutable);
                GlobalValue::Native(llvm_global.to_super())
            }
            Global::InModule {
                generated_name,
                content_type,
                mutable,
                initializer,
            } => {
                let v = initializer_to_value(llvm_ctx, content_type, &initializer);
                if opt.inline_constant_globals && !mutable {
                    GlobalValue::InlinedConstant(v)
                } else {
                    let llvm_global = llvm_module.add_global_variable(&generated_name, v);
                    llvm_global.set_constant(!mutable);
                    GlobalValue::Native(llvm_global.to_super())
                }
            }
        };
        global_values.push(v);
    }
    global_values
}

fn insert_runtime_globals<'a>(
    opt: &Opt,
    llvm_ctx: &'a LLVMCtx,
    llvm_module: &'a LLVMModule,
    globals: Vec<Global>,
) -> Vec<GlobalValue<'a>> {
    let setup_globals_type = FunctionType::new(<()>::get_type(llvm_ctx), &[]).to_super();
    let setup_globals = llvm_module.add_function("populate_globals", setup_globals_type);
    let bb = setup_globals.append("root");
    let b = Builder::new(llvm_ctx);
    b.position_at_end(bb);

    let mut global_values = Vec::new();
    for (i, g) in globals.into_iter().enumerate() {
        let v = match g {
            Global::Imported { .. } => {
                panic!("Cannot import globals into a module with runtime globals enabled")
            }
            Global::InModule {
                content_type,
                mutable,
                initializer,
                ..
            } => {
                let v = initializer_to_value(llvm_ctx, content_type, &initializer);
                if opt.inline_constant_globals && !mutable {
                    GlobalValue::InlinedConstant(v)
                } else {
                    let idx = i as u32;
                    match content_type {
                        Type::I32 => {
                            let func = llvm_module.get_function(SET_GLOBAL_I32).unwrap();
                            b.build_call(func, &[idx.compile(llvm_ctx), v]);
                            GlobalValue::RuntimeI32(idx)
                        }
                        Type::I64 => {
                            let func = llvm_module.get_function(SET_GLOBAL_I64).unwrap();
                            b.build_call(func, &[idx.compile(llvm_ctx), v]);
                            GlobalValue::RuntimeI64(idx)
                        }
                        e => panic!("Unimplemented runtime global type {:?}", e),
                    }
                }
            }
        };
        global_values.push(v);
    }
    b.build_ret_void();

    global_values
}

fn initializer_to_value<'a>(
    llvm_ctx: &'a LLVMCtx,
    content_type: Type,
    initializer: &[Instruction],
) -> &'a Value {
    assert_eq!(initializer.len(), 1);
    let v = match initializer[0] {
        Instruction::I32Const(i) => i.compile(llvm_ctx),
        Instruction::I64Const(i) => i.compile(llvm_ctx),
        Instruction::F32Const(f) => f.compile(llvm_ctx),
        Instruction::F64Const(f) => f.compile(llvm_ctx),
        ref e => panic!("Non simple initializer instruction {:?}", e),
    };
    let ty = llvm_type_to_wasm_type(llvm_ctx, v.get_type());
    assert_eq!(ty, content_type);

    v
}
