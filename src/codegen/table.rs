use llvm::Builder;
use llvm::Compile;
use llvm::FunctionType;
use llvm::PointerType;
use llvm::Sub;

use crate::wasm::TableInitializer;

use crate::codegen::memory::generate_offset_function;
use crate::codegen::ModuleCtx;

use crate::codegen::runtime_stubs::*;

pub fn generate_table_initialization_stub(m_ctx: &ModuleCtx, initializers: Vec<TableInitializer>) {
    let mut initialization_data: Vec<(&llvm::Function, Vec<u32>)> = Vec::new();

    for (n, i) in initializers.into_iter().enumerate() {
        // We need to translate the offset expression into a usable value
        // So we compile a function that evaluates the expression, and use that
        let offset_func = generate_offset_function(m_ctx, "table", n, i.offset_expression);

        initialization_data.push((&*offset_func, i.function_indexes));
    }

    let setup_function = m_ctx.llvm_module.add_function(
        "populate_table",
        FunctionType::new(<()>::get_type(m_ctx.llvm_ctx), &[]).to_super(),
    );
    let bb = setup_function.append("entry");
    let b = Builder::new(m_ctx.llvm_ctx);
    b.position_at_end(bb);

    for (offset_func, data) in initialization_data {
        let start_offset = b.build_call(offset_func, &[]);
        for (n, f_offset) in data.into_iter().enumerate() {
            let (llvm_f, ref wasm_f) = m_ctx.functions[f_offset as usize];

            let table_offset = b.build_add(start_offset, (n as u32).compile(m_ctx.llvm_ctx));
            let type_id = wasm_f.get_type_index().compile(m_ctx.llvm_ctx);
            let f_ptr = llvm_f.to_super();
            let f_ptr_as_u8 =
                b.build_bit_cast(f_ptr, PointerType::new(<u8>::get_type(m_ctx.llvm_ctx)));

            b.build_call(
                get_stub_function(m_ctx, TABLE_ADD),
                &[table_offset, type_id, f_ptr_as_u8],
            );
        }
    }
    b.build_ret_void();
}
