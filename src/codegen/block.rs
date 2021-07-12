use std::cell::Cell;

use llvm::BasicBlock;
use llvm::Builder;
use llvm::Compile;
use llvm::PointerType;
use llvm::Predicate;
use llvm::Value;

use wasmparser::Type;

use crate::codegen::ModuleCtx;

use crate::codegen::breakout::BreakoutTarget;
use crate::codegen::breakout::WBreakoutTarget;

use crate::codegen::function::FunctionCtx;

use crate::codegen::runtime_stubs::*;

use crate::codegen::type_conversions::llvm_type_to_wasm_type;
use crate::codegen::type_conversions::wasm_func_type_to_llvm_type;

use crate::wasm::Instruction;

pub struct IfCtx<'a, 'b> {
    else_block: &'a BasicBlock,
    else_locals: Vec<&'a Value>,
    else_built: &'b Cell<bool>
}

// TODO: Double check each instruction to make sure it does the right thing in both the safe and unsafe case
pub fn compile_block<'a, 'b, 'c>(
    m_ctx: &'a ModuleCtx,
    f_ctx: &'a FunctionCtx,
    breakout_stack: &mut Vec<WBreakoutTarget<'a>>,
    termination_target: &WBreakoutTarget<'a>,
    if_ctx: Option<IfCtx<'a, 'c>>,
    mut locals: Vec<&'a Value>,
    initial_bb: &'a BasicBlock,
    instructions: &'b [Instruction],
) -> &'b [Instruction] {
    // FIXME: If a block terminates, the remaining instructions should be deleted up to the next END instruction
    // This avoids so called "virtual" stack handling
    // Right now we just assume LLVM isn't stupid and won't emit useless instructions

    // We alias a few common fields so we don't have to type out a ton of stuff everytime we use them
    let b: &Builder = f_ctx.builder;

    // Each semantic "scope" has its own stack
    let mut stack: Vec<&Value> = Vec::new();

    // A single wasm block might need multiple llvm basic blocks to be expressed
    let mut basic_block = initial_bb;
    b.position_at_end(basic_block);

    // A block can be terminated, which changes the behavior of the "end" instruction
    // FIXME: Should we advance to an `end` instruction if we terminate the block??
    let mut block_terminated = false;

    let mut remaining_instructions = instructions;
    loop {
        let inst = remaining_instructions[0].clone();
        remaining_instructions = &remaining_instructions[1..];

        match inst {
            Instruction::BlockStart { produced_type } => {
                // The inner block must have a basic block to hold its code
                // TODO: Figure out if a block can just inherit its parent's basic block
                let inner_bb = f_ctx.generate_block();
                b.build_br(inner_bb);

                // An inner block inherts the locals of its parent
                let inner_locals = locals.clone();

                // The inner jump invalidates our old basic block, so we need a new one
                let after_bb = f_ctx.generate_block();

                // If the block breaks out, it will come back to our "after" bb, with it's produced type
                let breakout_target = BreakoutTarget::new_wrapped(after_bb, produced_type);
                breakout_stack.push(breakout_target.clone());

                // If the block terminates, it comes back to the "after" bb as well
                let inner_termination_target = breakout_target.clone();

                remaining_instructions = compile_block(
                    m_ctx,
                    f_ctx,
                    breakout_stack,
                    &inner_termination_target,
                    None,
                    inner_locals,
                    inner_bb,
                    remaining_instructions,
                );

                // Now we pop off the breakout stack
                let used_breakout_target = breakout_stack.pop().unwrap();

                // And rewrite our locals for our new bb
                basic_block = after_bb;
                b.position_at_end(basic_block);

                locals = used_breakout_target
                    .borrow()
                    .build_locals(m_ctx.llvm_ctx, b, &locals);

                // Also, if the block was supposed to yield a value, we collect it and add it onto the stack
                let result: Option<&Value> = used_breakout_target
                    .borrow()
                    .build_result(m_ctx.llvm_ctx, b);
                if let Some(result) = result {
                    stack.push(result)
                }
            }
            Instruction::LoopStart { produced_type } => {
                // The inner loop must have a basic block to hold its code
                let inner_bb = f_ctx.generate_block();
                b.build_br(inner_bb);

                // The locals of a loop have to be phi instructions, so they can be populated by breakouts later
                let mut inner_local_phis = Vec::new();
                b.position_at_end(inner_bb);
                for &l in &locals {
                    let phi = b.build_phi(vec![(basic_block, l)]);
                    inner_local_phis.push(phi);
                }
                // We need to do phi instruction modification, so we need to keep the phi vector. Thus we pass in a copy
                let inner_locals = inner_local_phis.clone();

                // The inner jump invalidates our old basic block, so we need a new one
                let after_bb = f_ctx.generate_block();

                // If the loop breaks out, it will go back to the top of the loop
                let loop_breakout_target = BreakoutTarget::new_wrapped(inner_bb, None);
                breakout_stack.push(loop_breakout_target);

                // If the loop terminates, it will come back to our outer basic block
                let loop_termination_target = BreakoutTarget::new_wrapped(after_bb, produced_type);

                remaining_instructions = compile_block(
                    m_ctx,
                    f_ctx,
                    breakout_stack,
                    &loop_termination_target.clone(),
                    None,
                    inner_locals,
                    inner_bb,
                    remaining_instructions,
                );

                // Now we pop off the breakout stack
                let used_loop_target = breakout_stack.pop().unwrap();
                // This target contains the information about who branches back to the top of the loop
                // We use this information to modify the phi instructions defining locals at the top of the loop
                used_loop_target.borrow().modify_phis(&inner_local_phis);

                // Handle the loop termination case, where it comes back to execute the rest of our block
                basic_block = after_bb;
                b.position_at_end(basic_block);

                locals = loop_termination_target
                    .borrow()
                    .build_locals(m_ctx.llvm_ctx, b, &locals);

                // Also, if the block was supposed to yield a value, we collect it and add it onto the stack
                let result: Option<&Value> = loop_termination_target
                    .borrow()
                    .build_result(m_ctx.llvm_ctx, b);
                if let Some(result) = result {
                    stack.push(result)
                }
            }
            Instruction::If { produced_type } => {
                // TODO: Figure out if the `if` block can just inherit its parent's basic block

                // The `if` part gets a block
                let if_branch_block = f_ctx.generate_block();
                let if_branch_locals = locals.clone();
                // So does the else part
                let else_branch_block = f_ctx.generate_block();
                let else_branch_locals = locals.clone();

                // The inner jump invalidates our old basic block, so we need a new one
                let after_bb = f_ctx.generate_block();

                // If the block breaks out, it will come back to our "after" bb, with it's produced type
                let breakout_target = BreakoutTarget::new_wrapped(after_bb, produced_type);
                breakout_stack.push(breakout_target.clone());

                // If the block terminates, it comes back to the "after" bb as well
                let inner_termination_target = breakout_target.clone();

                // Build the conditional jump
                let i32_v = stack.pop().unwrap();
                let v = is_non_zero_i32(m_ctx, b, i32_v);
                b.build_cond_br(v, if_branch_block, Some(else_branch_block));

                // Then compile what's inside the if statement. This will take us through the else to the end statement
                let else_built = Cell::new(false);
                remaining_instructions = compile_block(
                    m_ctx,
                    f_ctx,
                    breakout_stack,
                    &inner_termination_target,
                    Some(IfCtx {
                        else_block: else_branch_block,
                        else_locals: else_branch_locals.clone(),
                        else_built: &else_built
                    }),
                    if_branch_locals,
                    if_branch_block,
                    remaining_instructions,
                );

                // Handle the case where there is no else
                if !else_built.get() {
                    let fake_stack = Vec::new(); // Tacitly assumes an if with no else can't produce a value
                    let mut tt = inner_termination_target.borrow_mut();
                    tt.add_jump(else_branch_block, &else_branch_locals, &fake_stack);
                    b.position_at_end(else_branch_block);
                    b.build_br(tt.bb);
                }

                // Now we pop off the breakout stack
                let used_breakout_target = breakout_stack.pop().unwrap();

                // And rewrite our locals for our new bb
                basic_block = after_bb;
                b.position_at_end(basic_block);

                locals = used_breakout_target
                    .borrow()
                    .build_locals(m_ctx.llvm_ctx, b, &locals);

                // Also, if the block was supposed to yield a value, we collect it and add it onto the stack
                let result: Option<&Value> = used_breakout_target
                    .borrow()
                    .build_result(m_ctx.llvm_ctx, b);
                if let Some(result) = result {
                    stack.push(result)
                }
            }
            Instruction::Else => {
                // First terminate the `if` part of the equation, by adding a jump to the termination point
                // (which will always be the end of the `if`)
                let mut tt = termination_target.borrow_mut();
                tt.add_jump(basic_block, &locals, &stack);
                b.build_br(tt.bb);
                // Fetch the ctx
                let if_ctx_resolved = if_ctx.as_ref().expect("malformed wasm -- else with no if");
                // Ensure we don't build a duplicate else
                assert_eq!(if_ctx_resolved.else_built.get(), false);
                if_ctx_resolved.else_built.set(true);
                // Reset the stack / locals
                stack = Vec::new();
                locals = if_ctx_resolved.else_locals.clone();
                // Now move us on to the `else` part
                basic_block = if_ctx_resolved.else_block;
                b.position_at_end(basic_block);
            }
            Instruction::End => {
                if !block_terminated {
                    let mut tt = termination_target.borrow_mut();
                    tt.add_jump(basic_block, &locals, &stack);
                    b.build_br(tt.bb);
                }
                return remaining_instructions;
            }

            Instruction::Br { depth } => {
                let index = breakout_stack.len() - 1 - depth as usize;
                let mut tt = breakout_stack[index].borrow_mut();
                tt.add_jump(basic_block, &locals, &stack);
                b.build_br(tt.bb);
                block_terminated = true;
            }
            Instruction::BrIf { depth } => {
                let i32_v = stack.pop().unwrap();
                let v = is_non_zero_i32(m_ctx, b, i32_v);

                let index = breakout_stack.len() - 1 - depth as usize;
                let mut tt = breakout_stack[index].borrow_mut();
                tt.add_jump(basic_block, &locals, &stack);

                let else_block = f_ctx.generate_block();
                b.build_cond_br(v, tt.bb, Some(else_block));

                basic_block = else_block;
                b.position_at_end(basic_block);
            }
            Instruction::BrTable { table, default } => {
                let switch_value = stack.pop().unwrap();

                let mut jumps = Vec::new();
                jumps.push(default);
                jumps.extend(table.clone());

                for depth in jumps {
                    let index = breakout_stack.len() - 1 - depth as usize;
                    let mut tt = breakout_stack[index].borrow_mut();
                    tt.add_jump(basic_block, &locals, &stack);
                }

                let mut switch_options: Vec<(&Value, &BasicBlock)> = Vec::new();
                for (ind, depth) in table.into_iter().enumerate() {
                    let i = breakout_stack.len() - 1 - depth as usize;
                    switch_options.push((
                        (ind as i32).compile(m_ctx.llvm_ctx),
                        breakout_stack[i].borrow().bb,
                    ));
                }

                let default_jump = breakout_stack[breakout_stack.len() - 1 - default as usize]
                    .borrow()
                    .bb;
                b.build_switch(switch_value, default_jump, &switch_options);
                block_terminated = true;
            }

            Instruction::Return => {
                if f_ctx.has_return {
                    b.build_ret(stack.pop().unwrap());
                } else {
                    b.build_ret_void();
                }
                block_terminated = true;
            }
            Instruction::Unreachable => {
                // Can't build two unreachable instructions in a row, so avoid that
                if !block_terminated {
                    let trap_call = get_stub_function(m_ctx, TRAP);
                    b.build_call(trap_call, &[]);
                    b.build_unreachable();
                }

                block_terminated = true;
            }

            Instruction::Call { index } => {
                let &(llvm_f, ref wasm_f) = &m_ctx.functions[index as usize];

                let arg_count = wasm_f.count_args();
                let mut args = Vec::new();
                for _ in 0..arg_count {
                    args.push(stack.pop().unwrap());
                }
                // args are pushed in the opposite order of their use
                args.reverse();

                let result = b.build_call(llvm_f, &args);
                if wasm_f.has_return() {
                    stack.push(result);
                }
            }
            Instruction::CallIndirect { type_index } => {
                let table_index = stack.pop().unwrap();
                assert_type(m_ctx, table_index, Type::I32);

                let f_type = &m_ctx.types[type_index as usize];

                let arg_count = f_type.params.len();
                let has_return = match f_type.returns.len() {
                    0 => false,
                    1 => true,
                    _ => panic!(
                        "Malformed wasm, attempt to use a func type with more than one return"
                    ),
                };

                let mut args = Vec::new();
                for _ in 0..arg_count {
                    args.push(stack.pop().unwrap());
                }
                // args are pushed in the opposite order of their use
                args.reverse();

                // Fetch the func pointer from our table
                let f_ptr_as_void = b.build_call(
                    get_stub_function(m_ctx, TABLE_FETCH),
                    &[table_index, type_index.compile(m_ctx.llvm_ctx)],
                );
                // Then cast it from a void pointer to a function pointer
                let f_type = PointerType::new(wasm_func_type_to_llvm_type(m_ctx.llvm_ctx, f_type));
                let f_ptr = b.build_bit_cast(f_ptr_as_void, f_type);

                let result = b.build_value_call(f_ptr, &args);
                if has_return {
                    stack.push(result);
                }
            }
            Instruction::Drop => {
                stack.pop().unwrap();
            }
            Instruction::Nop => {}
            Instruction::Select => {
                let v3 = stack.pop().unwrap();
                let select_v = is_non_zero_i32(m_ctx, b, v3);

                let v2 = stack.pop().unwrap();
                let v1 = stack.pop().unwrap();
                assert_types_match(m_ctx, v1, v2);

                let res = b.build_select(select_v, v1, v2);
                stack.push(res);
            }

            Instruction::GetLocal { index } => {
                stack.push(locals[index as usize]);
            }
            Instruction::SetLocal { index } => {
                let v = stack.pop().unwrap();
                locals[index as usize] = v;
            }
            Instruction::TeeLocal { index } => {
                // Differs from `SetLocal` in that it doesn't pop it's operand off the stack
                let v = *stack.last().unwrap();
                locals[index as usize] = v;
            }

            Instruction::GetGlobal { index } => {
                let v = m_ctx.globals[index as usize].load(m_ctx, b);
                stack.push(v);
            }
            Instruction::SetGlobal { index } => {
                let v = stack.pop().unwrap();
                m_ctx.globals[index as usize].store(m_ctx, b, v);
            }

            Instruction::I32Const(i) => {
                let v = i.compile(m_ctx.llvm_ctx);
                stack.push(v);
            }

            Instruction::I32WrapI64 => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::I64);
                let result = b.build_trunc(v, <i32>::get_type(m_ctx.llvm_ctx));
                stack.push(result);
            }

            Instruction::I32ReinterpretF32 => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::F32);
                let result = b.build_bit_cast(v, <i32>::get_type(m_ctx.llvm_ctx));
                stack.push(result);
            }

            Instruction::I32TruncSF32 => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::F32);
                let result = if m_ctx.opt.use_fast_unsafe_implementations {
                    b.build_fptosi(v, <i32>::get_type(m_ctx.llvm_ctx))
                } else {
                    b.build_call(get_stub_function(m_ctx, I32_TRUNC_F32), &[v])
                };
                stack.push(result);
            }
            Instruction::I32TruncUF32 => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::F32);
                let result = if m_ctx.opt.use_fast_unsafe_implementations {
                    b.build_fptoui(v, <u32>::get_type(m_ctx.llvm_ctx))
                } else {
                    b.build_call(get_stub_function(m_ctx, U32_TRUNC_F32), &[v])
                };
                stack.push(result);
            }
            Instruction::I32TruncSF64 => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::F64);
                let result = if m_ctx.opt.use_fast_unsafe_implementations {
                    b.build_fptosi(v, <i32>::get_type(m_ctx.llvm_ctx))
                } else {
                    b.build_call(get_stub_function(m_ctx, I32_TRUNC_F64), &[v])
                };
                stack.push(result);
            }
            Instruction::I32TruncUF64 => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::F64);
                let result = if m_ctx.opt.use_fast_unsafe_implementations {
                    b.build_fptoui(v, <u32>::get_type(m_ctx.llvm_ctx))
                } else {
                    b.build_call(get_stub_function(m_ctx, I32_TRUNC_F64), &[v])
                };
                stack.push(result);
            }

            Instruction::I32Add => {
                perform_bin_op(m_ctx, &mut stack, Type::I32, |v1, v2| b.build_add(v1, v2));
            }
            Instruction::I32And => {
                perform_bin_op(m_ctx, &mut stack, Type::I32, |v1, v2| b.build_and(v1, v2));
            }
            Instruction::I32Clz => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::I32);

                let is_zero_undef = false;
                let flag = is_zero_undef.compile(m_ctx.llvm_ctx);

                let result = b.build_call(get_stub_function(m_ctx, I32_CLZ), &[v, flag]);
                stack.push(result);
            }
            Instruction::I32Ctz => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::I32);

                let is_zero_undef = false;
                let flag = is_zero_undef.compile(m_ctx.llvm_ctx);

                let result = b.build_call(get_stub_function(m_ctx, I32_CTZ), &[v, flag]);
                stack.push(result);
            }
            Instruction::I32DivS => {
                perform_bin_op(m_ctx, &mut stack, Type::I32, |v1, v2| {
                    if m_ctx.opt.use_fast_unsafe_implementations {
                        b.build_div(v1, v2)
                    } else {
                        b.build_call(get_stub_function(m_ctx, I32_DIV), &[v1, v2])
                    }
                });
            }
            Instruction::I32DivU => {
                perform_bin_op(m_ctx, &mut stack, Type::I32, |v1, v2| {
                    if m_ctx.opt.use_fast_unsafe_implementations {
                        b.build_udiv(v1, v2)
                    } else {
                        b.build_call(get_stub_function(m_ctx, U32_DIV), &[v1, v2])
                    }
                });
            }
            Instruction::I32Mul => {
                perform_bin_op(m_ctx, &mut stack, Type::I32, |v1, v2| b.build_mul(v1, v2));
            }
            Instruction::I32Or => {
                perform_bin_op(m_ctx, &mut stack, Type::I32, |v1, v2| b.build_or(v1, v2));
            }
            Instruction::I32Popcnt => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::I32);
                let result = b.build_call(get_stub_function(m_ctx, I32_CTPOP), &[v]);
                stack.push(result);
            }
            Instruction::I32RemS => {
                perform_bin_op(m_ctx, &mut stack, Type::I32, |v1, v2| {
                    if m_ctx.opt.use_fast_unsafe_implementations {
                        b.build_srem(v1, v2)
                    } else {
                        b.build_call(get_stub_function(m_ctx, I32_REM), &[v1, v2])
                    }
                });
            }
            Instruction::I32RemU => {
                perform_bin_op(m_ctx, &mut stack, Type::I32, |v1, v2| {
                    if m_ctx.opt.use_fast_unsafe_implementations {
                        b.build_urem(v1, v2)
                    } else {
                        b.build_call(get_stub_function(m_ctx, U32_REM), &[v1, v2])
                    }
                });
            }
            Instruction::I32Rotl => {
                let v2 = stack.pop().unwrap();
                let v1 = stack.pop().unwrap();
                assert_types(m_ctx, v1, v2, Type::I32);
                let result = b.build_call(get_stub_function(m_ctx, I32_ROTL), &[v1, v2]);
                stack.push(result);
            }
            Instruction::I32Rotr => {
                let v2 = stack.pop().unwrap();
                let v1 = stack.pop().unwrap();
                assert_types(m_ctx, v1, v2, Type::I32);
                let result = b.build_call(get_stub_function(m_ctx, I32_ROTR), &[v1, v2]);
                stack.push(result);
            }
            Instruction::I32Shl => {
                perform_bin_op(m_ctx, &mut stack, Type::I32, |v1, v2| {
                    if m_ctx.opt.use_fast_unsafe_implementations {
                        b.build_shl(v1, v2)
                    } else {
                        let bit_count = (32i32).compile(m_ctx.llvm_ctx);
                        let corrected_shift = b.build_urem(v2, bit_count);
                        b.build_shl(v1, corrected_shift)
                    }
                });
            }
            Instruction::I32ShrS => {
                perform_bin_op(m_ctx, &mut stack, Type::I32, |v1, v2| {
                    if m_ctx.opt.use_fast_unsafe_implementations {
                        b.build_ashr(v1, v2)
                    } else {
                        let bit_count = (32i32).compile(m_ctx.llvm_ctx);
                        let corrected_shift = b.build_urem(v2, bit_count);
                        b.build_ashr(v1, corrected_shift)
                    }
                });
            }
            Instruction::I32ShrU => {
                perform_bin_op(m_ctx, &mut stack, Type::I32, |v1, v2| {
                    if m_ctx.opt.use_fast_unsafe_implementations {
                        b.build_lshr(v1, v2)
                    } else {
                        let bit_count = (32i32).compile(m_ctx.llvm_ctx);
                        let corrected_shift = b.build_urem(v2, bit_count);
                        b.build_lshr(v1, corrected_shift)
                    }
                });
            }
            Instruction::I32Sub => {
                perform_bin_op(m_ctx, &mut stack, Type::I32, |v1, v2| b.build_sub(v1, v2));
            }
            Instruction::I32Xor => {
                perform_bin_op(m_ctx, &mut stack, Type::I32, |v1, v2| b.build_xor(v1, v2));
            }

            Instruction::I32Eqz => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::I32);
                let result =
                    b.build_unsigned_cmp(0i32.compile(m_ctx.llvm_ctx), v, Predicate::Equal);
                let i32_result = b.build_zext(result, <i32>::get_type(m_ctx.llvm_ctx));
                stack.push(i32_result);
            }
            Instruction::I32Eq => i32_cmp_signed(m_ctx, b, &mut stack, Predicate::Equal),
            Instruction::I32Ne => i32_cmp_signed(m_ctx, b, &mut stack, Predicate::NotEqual),
            Instruction::I32LeS => i32_cmp_signed(m_ctx, b, &mut stack, Predicate::LessThanOrEqual),
            Instruction::I32LeU => {
                i32_cmp_unsigned(m_ctx, b, &mut stack, Predicate::LessThanOrEqual)
            }
            Instruction::I32LtS => i32_cmp_signed(m_ctx, b, &mut stack, Predicate::LessThan),
            Instruction::I32LtU => i32_cmp_unsigned(m_ctx, b, &mut stack, Predicate::LessThan),
            Instruction::I32GeS => {
                i32_cmp_signed(m_ctx, b, &mut stack, Predicate::GreaterThanOrEqual)
            }
            Instruction::I32GeU => {
                i32_cmp_unsigned(m_ctx, b, &mut stack, Predicate::GreaterThanOrEqual)
            }
            Instruction::I32GtS => i32_cmp_signed(m_ctx, b, &mut stack, Predicate::GreaterThan),
            Instruction::I32GtU => i32_cmp_unsigned(m_ctx, b, &mut stack, Predicate::GreaterThan),

            Instruction::I64Const(i) => {
                let v = i.compile(m_ctx.llvm_ctx);
                stack.push(v);
            }

            Instruction::I64ExtendSI32 => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::I32);
                let result = b.build_zext(v, <i64>::get_type(m_ctx.llvm_ctx));
                stack.push(result);
            }
            Instruction::I64ExtendUI32 => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::I32);
                let result = b.build_sext(v, <i64>::get_type(m_ctx.llvm_ctx));
                stack.push(result);
            }

            Instruction::I64ReinterpretF64 => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::F64);
                let result = b.build_bit_cast(v, <i64>::get_type(m_ctx.llvm_ctx));
                stack.push(result);
            }

            Instruction::I64TruncSF32 => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::F32);
                let result = b.build_call(get_stub_function(m_ctx, I64_TRUNC_F32), &[v]);
                stack.push(result);
            }
            Instruction::I64TruncUF32 => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::F32);
                let result = b.build_call(get_stub_function(m_ctx, U64_TRUNC_F32), &[v]);
                stack.push(result);
            }
            Instruction::I64TruncSF64 => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::F64);
                let result = b.build_call(get_stub_function(m_ctx, I64_TRUNC_F64), &[v]);
                stack.push(result);
            }
            Instruction::I64TruncUF64 => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::F64);
                let result = b.build_call(get_stub_function(m_ctx, U64_TRUNC_F64), &[v]);
                stack.push(result);
            }

            Instruction::I64Add => {
                perform_bin_op(m_ctx, &mut stack, Type::I64, |v1, v2| b.build_add(v1, v2));
            }
            Instruction::I64And => {
                perform_bin_op(m_ctx, &mut stack, Type::I64, |v1, v2| b.build_and(v1, v2));
            }
            Instruction::I64Clz => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::I64);

                let is_zero_undef = false;
                let flag = is_zero_undef.compile(m_ctx.llvm_ctx);

                let result = b.build_call(get_stub_function(m_ctx, I64_CLZ), &[v, flag]);
                stack.push(result);
            }
            Instruction::I64Ctz => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::I64);

                let is_zero_undef = false;
                let flag = is_zero_undef.compile(m_ctx.llvm_ctx);

                let result = b.build_call(get_stub_function(m_ctx, I64_CTZ), &[v, flag]);
                stack.push(result);
            }
            Instruction::I64DivS => {
                perform_bin_op(m_ctx, &mut stack, Type::I64, |v1, v2| {
                    if m_ctx.opt.use_fast_unsafe_implementations {
                        b.build_div(v1, v2)
                    } else {
                        b.build_call(get_stub_function(m_ctx, I64_DIV), &[v1, v2])
                    }
                });
            }
            Instruction::I64DivU => {
                perform_bin_op(m_ctx, &mut stack, Type::I64, |v1, v2| {
                    if m_ctx.opt.use_fast_unsafe_implementations {
                        b.build_udiv(v1, v2)
                    } else {
                        b.build_call(get_stub_function(m_ctx, U64_DIV), &[v1, v2])
                    }
                });
            }
            Instruction::I64Mul => {
                perform_bin_op(m_ctx, &mut stack, Type::I64, |v1, v2| b.build_mul(v1, v2));
            }
            Instruction::I64Or => {
                perform_bin_op(m_ctx, &mut stack, Type::I64, |v1, v2| b.build_or(v1, v2));
            }
            Instruction::I64Popcnt => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::I64);
                let result = b.build_call(get_stub_function(m_ctx, I64_CTPOP), &[v]);
                stack.push(result);
            }
            Instruction::I64RemS => {
                perform_bin_op(m_ctx, &mut stack, Type::I64, |v1, v2| {
                    if m_ctx.opt.use_fast_unsafe_implementations {
                        b.build_srem(v1, v2)
                    } else {
                        b.build_call(get_stub_function(m_ctx, I64_REM), &[v1, v2])
                    }
                });
            }
            Instruction::I64RemU => {
                perform_bin_op(m_ctx, &mut stack, Type::I64, |v1, v2| {
                    if m_ctx.opt.use_fast_unsafe_implementations {
                        b.build_urem(v1, v2)
                    } else {
                        b.build_call(get_stub_function(m_ctx, U64_REM), &[v1, v2])
                    }
                });
            }
            Instruction::I64Rotl => {
                let v2 = stack.pop().unwrap();
                let v1 = stack.pop().unwrap();
                assert_types(m_ctx, v1, v2, Type::I64);
                let result = b.build_call(get_stub_function(m_ctx, I64_ROTL), &[v1, v2]);
                stack.push(result);
            }
            Instruction::I64Rotr => {
                let v2 = stack.pop().unwrap();
                let v1 = stack.pop().unwrap();
                assert_types(m_ctx, v1, v2, Type::I64);
                let result = b.build_call(get_stub_function(m_ctx, I64_ROTR), &[v1, v2]);
                stack.push(result);
            }
            Instruction::I64Shl => {
                perform_bin_op(m_ctx, &mut stack, Type::I64, |v1, v2| {
                    if m_ctx.opt.use_fast_unsafe_implementations {
                        b.build_shl(v1, v2)
                    } else {
                        let bit_count = (64i64).compile(m_ctx.llvm_ctx);
                        let corrected_shift = b.build_urem(v2, bit_count);
                        b.build_shl(v1, corrected_shift)
                    }
                });
            }
            Instruction::I64ShrS => {
                perform_bin_op(m_ctx, &mut stack, Type::I64, |v1, v2| {
                    if m_ctx.opt.use_fast_unsafe_implementations {
                        b.build_ashr(v1, v2)
                    } else {
                        let bit_count = (64i64).compile(m_ctx.llvm_ctx);
                        let corrected_shift = b.build_urem(v2, bit_count);
                        b.build_ashr(v1, corrected_shift)
                    }
                });
            }
            Instruction::I64ShrU => {
                perform_bin_op(m_ctx, &mut stack, Type::I64, |v1, v2| {
                    if m_ctx.opt.use_fast_unsafe_implementations {
                        b.build_lshr(v1, v2)
                    } else {
                        let bit_count = (64i64).compile(m_ctx.llvm_ctx);
                        let corrected_shift = b.build_urem(v2, bit_count);
                        b.build_lshr(v1, corrected_shift)
                    }
                });
            }
            Instruction::I64Sub => {
                perform_bin_op(m_ctx, &mut stack, Type::I64, |v1, v2| b.build_sub(v1, v2));
            }
            Instruction::I64Xor => {
                perform_bin_op(m_ctx, &mut stack, Type::I64, |v1, v2| b.build_xor(v1, v2));
            }

            Instruction::I64Eqz => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::I64);
                let result =
                    b.build_unsigned_cmp(0i64.compile(m_ctx.llvm_ctx), v, Predicate::Equal);
                let i32_result = b.build_zext(result, <i32>::get_type(m_ctx.llvm_ctx));
                stack.push(i32_result);
            }
            Instruction::I64Eq => i64_cmp_signed(m_ctx, b, &mut stack, Predicate::Equal),
            Instruction::I64Ne => i64_cmp_signed(m_ctx, b, &mut stack, Predicate::NotEqual),
            Instruction::I64LeS => i64_cmp_signed(m_ctx, b, &mut stack, Predicate::LessThanOrEqual),
            Instruction::I64LeU => i64_cmp_unsigned(m_ctx, b, &mut stack, Predicate::LessThan),
            Instruction::I64LtS => i64_cmp_signed(m_ctx, b, &mut stack, Predicate::LessThan),
            Instruction::I64LtU => i64_cmp_unsigned(m_ctx, b, &mut stack, Predicate::LessThan),
            Instruction::I64GeS => {
                i64_cmp_signed(m_ctx, b, &mut stack, Predicate::GreaterThanOrEqual)
            }
            Instruction::I64GeU => {
                i64_cmp_unsigned(m_ctx, b, &mut stack, Predicate::GreaterThanOrEqual)
            }
            Instruction::I64GtS => i64_cmp_signed(m_ctx, b, &mut stack, Predicate::GreaterThan),
            Instruction::I64GtU => i64_cmp_unsigned(m_ctx, b, &mut stack, Predicate::GreaterThan),

            Instruction::F32Const(i) => {
                let v = i.compile(m_ctx.llvm_ctx);
                stack.push(v);
            }

            Instruction::F32DemoteF64 => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::F64);
                let result = b.build_fptrunc(v, <f32>::get_type(m_ctx.llvm_ctx));
                stack.push(result);
            }

            Instruction::F32ReinterpretI32 => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::I32);
                let result = b.build_bit_cast(v, <f32>::get_type(m_ctx.llvm_ctx));
                stack.push(result);
            }

            Instruction::F32ConvertSI32 => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::I32);
                let result = b.build_sitofp(v, <f32>::get_type(m_ctx.llvm_ctx));
                stack.push(result);
            }
            Instruction::F32ConvertUI32 => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::I32);
                let result = b.build_uitofp(v, <f32>::get_type(m_ctx.llvm_ctx));
                stack.push(result);
            }
            Instruction::F32ConvertSI64 => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::I64);
                let result = b.build_sitofp(v, <f32>::get_type(m_ctx.llvm_ctx));
                stack.push(result);
            }
            Instruction::F32ConvertUI64 => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::I64);
                let result = b.build_uitofp(v, <f32>::get_type(m_ctx.llvm_ctx));
                stack.push(result);
            }

            Instruction::F32Abs => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::F32);
                let result = b.build_call(get_stub_function(m_ctx, F32_FABS), &[v]);
                stack.push(result);
            }
            Instruction::F32Add => {
                perform_bin_op(m_ctx, &mut stack, Type::F32, |v1, v2| b.build_add(v1, v2));
            }
            Instruction::F32Div => {
                perform_bin_op(m_ctx, &mut stack, Type::F32, |v1, v2| b.build_div(v1, v2));
            }
            Instruction::F32Mul => {
                perform_bin_op(m_ctx, &mut stack, Type::F32, |v1, v2| b.build_mul(v1, v2));
            }
            Instruction::F32Neg => {
                let v = stack.pop().unwrap();
                let result = b.build_fneg(v);
                stack.push(result);
            }
            Instruction::F32Sub => {
                perform_bin_op(m_ctx, &mut stack, Type::F32, |v1, v2| b.build_sub(v1, v2));
            }
            Instruction::F32Sqrt => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::F32);
                let result = b.build_call(get_stub_function(m_ctx, F32_SQRT), &[v]);
                stack.push(result);
            }
            Instruction::F32Trunc => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::F32);
                let result = b.build_call(get_stub_function(m_ctx, F32_TRUNC_F32), &[v]);
                stack.push(result);
            }

            Instruction::F32Eq => float_cmp(m_ctx, b, &mut stack, Predicate::Equal),
            Instruction::F32Ne => float_cmp(m_ctx, b, &mut stack, Predicate::NotEqual),
            Instruction::F32Le => float_cmp(m_ctx, b, &mut stack, Predicate::LessThanOrEqual),
            Instruction::F32Lt => float_cmp(m_ctx, b, &mut stack, Predicate::LessThan),
            Instruction::F32Ge => float_cmp(m_ctx, b, &mut stack, Predicate::GreaterThanOrEqual),
            Instruction::F32Gt => float_cmp(m_ctx, b, &mut stack, Predicate::GreaterThan),

            Instruction::F32Min => perform_bin_op(m_ctx, &mut stack, Type::F32, |v1, v2| {
                b.build_call(get_stub_function(m_ctx, F32_MIN), &[v1, v2])
            }),
            Instruction::F32Max => perform_bin_op(m_ctx, &mut stack, Type::F32, |v1, v2| {
                b.build_call(get_stub_function(m_ctx, F32_MAX), &[v1, v2])
            }),
            Instruction::F32Copysign => perform_bin_op(m_ctx, &mut stack, Type::F32, |v1, v2| {
                b.build_call(get_stub_function(m_ctx, F32_COPYSIGN), &[v1, v2])
            }),
            Instruction::F32Floor => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::F32);
                let result = b.build_call(get_stub_function(m_ctx, F32_FLOOR), &[v]);
                stack.push(result);
            }
            Instruction::F32Ceil => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::F32);
                let result = b.build_call(get_stub_function(m_ctx, F32_CEIL), &[v]);
                stack.push(result);
            }
            Instruction::F32Nearest => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::F32);
                let result = b.build_call(get_stub_function(m_ctx, F32_NEAREST), &[v]);
                stack.push(result);
            }

            Instruction::F64Const(i) => {
                let v = i.compile(m_ctx.llvm_ctx);
                stack.push(v);
            }

            Instruction::F64PromoteF32 => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::F32);
                let result = b.build_fpext(v, <f64>::get_type(m_ctx.llvm_ctx));
                stack.push(result);
            }

            Instruction::F64ReinterpretI64 => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::I64);
                let result = b.build_bit_cast(v, <f64>::get_type(m_ctx.llvm_ctx));
                stack.push(result);
            }

            Instruction::F64ConvertSI32 => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::I32);
                let result = b.build_sitofp(v, <f64>::get_type(m_ctx.llvm_ctx));
                stack.push(result);
            }
            Instruction::F64ConvertUI32 => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::I32);
                let result = b.build_uitofp(v, <f64>::get_type(m_ctx.llvm_ctx));
                stack.push(result);
            }
            Instruction::F64ConvertSI64 => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::I64);
                let result = b.build_sitofp(v, <f64>::get_type(m_ctx.llvm_ctx));
                stack.push(result);
            }
            Instruction::F64ConvertUI64 => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::I64);
                let result = b.build_uitofp(v, <f64>::get_type(m_ctx.llvm_ctx));
                stack.push(result);
            }

            Instruction::F64Abs => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::F64);
                let result = b.build_call(get_stub_function(m_ctx, F64_FABS), &[v]);
                stack.push(result);
            }
            Instruction::F64Add => {
                perform_bin_op(m_ctx, &mut stack, Type::F64, |v1, v2| b.build_add(v1, v2));
            }
            Instruction::F64Div => {
                perform_bin_op(m_ctx, &mut stack, Type::F64, |v1, v2| b.build_div(v1, v2));
            }
            Instruction::F64Mul => {
                perform_bin_op(m_ctx, &mut stack, Type::F64, |v1, v2| b.build_mul(v1, v2));
            }
            Instruction::F64Neg => {
                let v = stack.pop().unwrap();
                let result = b.build_fneg(v);
                stack.push(result);
            }
            Instruction::F64Sub => {
                perform_bin_op(m_ctx, &mut stack, Type::F64, |v1, v2| b.build_sub(v1, v2));
            }
            Instruction::F64Sqrt => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::F64);
                let result = b.build_call(get_stub_function(m_ctx, F64_SQRT), &[v]);
                stack.push(result);
            }
            Instruction::F64Trunc => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::F64);
                let result = b.build_call(get_stub_function(m_ctx, F64_TRUNC_F64), &[v]);
                stack.push(result);
            }

            Instruction::F64Eq => float_cmp(m_ctx, b, &mut stack, Predicate::Equal),
            Instruction::F64Ne => float_cmp(m_ctx, b, &mut stack, Predicate::NotEqual),
            Instruction::F64Le => float_cmp(m_ctx, b, &mut stack, Predicate::LessThanOrEqual),
            Instruction::F64Lt => float_cmp(m_ctx, b, &mut stack, Predicate::LessThan),
            Instruction::F64Ge => float_cmp(m_ctx, b, &mut stack, Predicate::GreaterThanOrEqual),
            Instruction::F64Gt => float_cmp(m_ctx, b, &mut stack, Predicate::GreaterThan),

            Instruction::F64Min => perform_bin_op(m_ctx, &mut stack, Type::F64, |v1, v2| {
                b.build_call(get_stub_function(m_ctx, F64_MIN), &[v1, v2])
            }),
            Instruction::F64Max => perform_bin_op(m_ctx, &mut stack, Type::F64, |v1, v2| {
                b.build_call(get_stub_function(m_ctx, F64_MAX), &[v1, v2])
            }),
            Instruction::F64Copysign => perform_bin_op(m_ctx, &mut stack, Type::F64, |v1, v2| {
                b.build_call(get_stub_function(m_ctx, F64_COPYSIGN), &[v1, v2])
            }),
            Instruction::F64Floor => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::F64);
                let result = b.build_call(get_stub_function(m_ctx, F64_FLOOR), &[v]);
                stack.push(result);
            }
            Instruction::F64Ceil => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::F64);
                let result = b.build_call(get_stub_function(m_ctx, F64_CEIL), &[v]);
                stack.push(result);
            }
            Instruction::F64Nearest => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::F64);
                let result = b.build_call(get_stub_function(m_ctx, F64_NEAREST), &[v]);
                stack.push(result);
            }

            Instruction::I32Load { offset, .. } => {
                let v = load_val::<i32>(m_ctx, b, &mut stack, offset);
                stack.push(v);
            }
            Instruction::I32Store { offset, .. } => {
                let v = stack.pop().unwrap();
                store_val::<i32>(m_ctx, b, &mut stack, offset, v);
            }
            Instruction::I32Load8S { offset, .. } => {
                load_as_i32_sext::<i8>(m_ctx, b, &mut stack, offset);
            }
            Instruction::I32Load8U { offset, .. } => {
                load_as_i32_zext::<u8>(m_ctx, b, &mut stack, offset);
            }
            Instruction::I32Store8 { offset, .. } => {
                let i32_v = stack.pop().unwrap();
                let v = b.build_trunc(i32_v, <u8>::get_type(m_ctx.llvm_ctx));
                store_val::<u8>(m_ctx, b, &mut stack, offset, v);
            }
            Instruction::I32Load16S { offset, .. } => {
                load_as_i32_sext::<i16>(m_ctx, b, &mut stack, offset);
            }
            Instruction::I32Load16U { offset, .. } => {
                load_as_i32_zext::<u16>(m_ctx, b, &mut stack, offset);
            }
            Instruction::I32Store16 { offset, .. } => {
                let i32_v = stack.pop().unwrap();
                let v = b.build_trunc(i32_v, <u16>::get_type(m_ctx.llvm_ctx));
                store_val::<u16>(m_ctx, b, &mut stack, offset, v);
            }

            Instruction::I64Load { offset, .. } => {
                let v = load_val::<i64>(m_ctx, b, &mut stack, offset);
                stack.push(v);
            }
            Instruction::I64Store { offset, .. } => {
                let v = stack.pop().unwrap();
                store_val::<i64>(m_ctx, b, &mut stack, offset, v);
            }
            Instruction::I64Load8S { offset, .. } => {
                load_as_i64_sext::<i8>(m_ctx, b, &mut stack, offset);
            }
            Instruction::I64Load8U { offset, .. } => {
                load_as_i64_zext::<u8>(m_ctx, b, &mut stack, offset);
            }
            Instruction::I64Store8 { offset, .. } => {
                let i64_v = stack.pop().unwrap();
                let v = b.build_trunc(i64_v, <u8>::get_type(m_ctx.llvm_ctx));
                store_val::<u8>(m_ctx, b, &mut stack, offset, v);
            }
            Instruction::I64Load16S { offset, .. } => {
                load_as_i64_sext::<i16>(m_ctx, b, &mut stack, offset);
            }
            Instruction::I64Load16U { offset, .. } => {
                load_as_i64_zext::<u16>(m_ctx, b, &mut stack, offset);
            }
            Instruction::I64Store16 { offset, .. } => {
                let i64_v = stack.pop().unwrap();
                let v = b.build_trunc(i64_v, <u16>::get_type(m_ctx.llvm_ctx));
                store_val::<u16>(m_ctx, b, &mut stack, offset, v);
            }
            Instruction::I64Load32S { offset, .. } => {
                load_as_i64_sext::<i32>(m_ctx, b, &mut stack, offset);
            }
            Instruction::I64Load32U { offset, .. } => {
                load_as_i64_zext::<u32>(m_ctx, b, &mut stack, offset);
            }
            Instruction::I64Store32 { offset, .. } => {
                let i64_v = stack.pop().unwrap();
                let v = b.build_trunc(i64_v, <u32>::get_type(m_ctx.llvm_ctx));
                store_val::<u32>(m_ctx, b, &mut stack, offset, v);
            }

            Instruction::F32Load { offset, .. } => {
                let v = load_val::<f32>(m_ctx, b, &mut stack, offset);
                stack.push(v);
            }
            Instruction::F32Store { offset, .. } => {
                let v = stack.pop().unwrap();
                store_val::<f32>(m_ctx, b, &mut stack, offset, v);
            }

            Instruction::F64Load { offset, .. } => {
                let v = load_val::<f64>(m_ctx, b, &mut stack, offset);
                stack.push(v);
            }
            Instruction::F64Store { offset, .. } => {
                let v = stack.pop().unwrap();
                store_val::<f64>(m_ctx, b, &mut stack, offset, v);
            }

            Instruction::MemorySize => {
                let result = b.build_call(get_stub_function(m_ctx, MEMORY_SIZE), &[]);
                stack.push(result);
            }
            Instruction::MemoryGrow => {
                let v = stack.pop().unwrap();
                assert_type(m_ctx, v, Type::I32);
                let result = b.build_call(get_stub_function(m_ctx, MEMORY_GROW), &[v]);
                stack.push(result);
            }
        }
    }
}

fn assert_type(m_ctx: &ModuleCtx, v: &Value, t: Type) {
    let value_type = llvm_type_to_wasm_type(m_ctx.llvm_ctx, v.get_type());
    assert_eq!(value_type, t);
}

fn assert_types(m_ctx: &ModuleCtx, v1: &Value, v2: &Value, t: Type) {
    let v1_type = llvm_type_to_wasm_type(m_ctx.llvm_ctx, v1.get_type());
    assert_eq!(v1_type, t);

    let v2_type = llvm_type_to_wasm_type(m_ctx.llvm_ctx, v2.get_type());
    assert_eq!(v2_type, t);
}

fn assert_types_match(m_ctx: &ModuleCtx, v1: &Value, v2: &Value) {
    let t1 = llvm_type_to_wasm_type(m_ctx.llvm_ctx, v1.get_type());
    let t2 = llvm_type_to_wasm_type(m_ctx.llvm_ctx, v2.get_type());
    assert_eq!(t1, t2);
}

fn is_non_zero_i32<'a>(m_ctx: &'a ModuleCtx, b: &'a Builder, v: &'a Value) -> &'a Value {
    assert_type(m_ctx, v, Type::I32);
    b.build_unsigned_cmp(v, 0i32.compile(m_ctx.llvm_ctx), Predicate::NotEqual)
}

fn perform_bin_op<'a, F: FnOnce(&'a Value, &'a Value) -> &'a Value>(
    m_ctx: &ModuleCtx,
    stack: &mut Vec<&'a Value>,
    ty: Type,
    f: F,
) {
    let v2 = stack.pop().unwrap();
    let v1 = stack.pop().unwrap();
    assert_types(m_ctx, v1, v2, ty);

    let result = f(v1, v2);
    stack.push(result);
}

fn i32_cmp_signed<'a>(
    m_ctx: &'a ModuleCtx,
    b: &'a Builder,
    stack: &mut Vec<&'a Value>,
    p: Predicate,
) {
    let v2 = stack.pop().unwrap();
    let v1 = stack.pop().unwrap();
    assert_types(m_ctx, v1, v2, Type::I32);

    let result = b.build_signed_cmp(v1, v2, p);
    let i32_result = b.build_zext(result, <i32>::get_type(m_ctx.llvm_ctx));
    stack.push(i32_result);
}

fn i32_cmp_unsigned<'a>(
    m_ctx: &'a ModuleCtx,
    b: &'a Builder,
    stack: &mut Vec<&'a Value>,
    p: Predicate,
) {
    let v2 = stack.pop().unwrap();
    let v1 = stack.pop().unwrap();
    assert_types(m_ctx, v1, v2, Type::I32);

    let result = b.build_unsigned_cmp(v1, v2, p);
    let i32_result = b.build_zext(result, <i32>::get_type(m_ctx.llvm_ctx));
    stack.push(i32_result);
}

fn i64_cmp_signed<'a>(
    m_ctx: &'a ModuleCtx,
    b: &'a Builder,
    stack: &mut Vec<&'a Value>,
    p: Predicate,
) {
    let v2 = stack.pop().unwrap();
    let v1 = stack.pop().unwrap();
    assert_types(m_ctx, v1, v2, Type::I64);

    let result = b.build_signed_cmp(v1, v2, p);
    let i32_result = b.build_zext(result, <i32>::get_type(m_ctx.llvm_ctx));
    stack.push(i32_result);
}

fn i64_cmp_unsigned<'a>(
    m_ctx: &'a ModuleCtx,
    b: &'a Builder,
    stack: &mut Vec<&'a Value>,
    p: Predicate,
) {
    let v2 = stack.pop().unwrap();
    let v1 = stack.pop().unwrap();
    assert_types(m_ctx, v1, v2, Type::I64);

    let result = b.build_unsigned_cmp(v1, v2, p);
    let i32_result = b.build_zext(result, <i32>::get_type(m_ctx.llvm_ctx));
    stack.push(i32_result);
}

fn float_cmp<'a>(m_ctx: &'a ModuleCtx, b: &'a Builder, stack: &mut Vec<&'a Value>, p: Predicate) {
    let v2 = stack.pop().unwrap();
    let v1 = stack.pop().unwrap();
    assert_types_match(m_ctx, v1, v2);

    let result = b.build_signed_cmp(v1, v2, p);
    let i32_result = b.build_zext(result, <i32>::get_type(m_ctx.llvm_ctx));
    stack.push(i32_result);
}

fn load_val<'a, L: Compile<'a>>(
    m_ctx: &'a ModuleCtx,
    b: &'a Builder,
    stack: &mut Vec<&'a Value>,
    offset: u32,
) -> &'a Value {
    let stack_offset = stack.pop().unwrap();
    let total_offset = b.build_add(stack_offset, offset.compile(m_ctx.llvm_ctx));

    let ty = L::get_type(m_ctx.llvm_ctx);
    let f = if ty == <i8>::get_type(m_ctx.llvm_ctx) {
        get_stub_function(m_ctx, GET_I8)
    } else if ty == <i16>::get_type(m_ctx.llvm_ctx) {
        get_stub_function(m_ctx, GET_I16)
    } else if ty == <i32>::get_type(m_ctx.llvm_ctx) {
        get_stub_function(m_ctx, GET_I32)
    } else if ty == <i64>::get_type(m_ctx.llvm_ctx) {
        get_stub_function(m_ctx, GET_I64)
    } else if ty == <f32>::get_type(m_ctx.llvm_ctx) {
        get_stub_function(m_ctx, GET_F32)
    } else if ty == <f64>::get_type(m_ctx.llvm_ctx) {
        get_stub_function(m_ctx, GET_F64)
    } else {
        panic!("cannot load value of type {:?}", ty)
    };
    b.build_call(f, &[total_offset])
}

fn load_as_i32_sext<'a, L: Compile<'a>>(
    m_ctx: &'a ModuleCtx,
    b: &'a Builder,
    stack: &mut Vec<&'a Value>,
    offset: u32,
) {
    let val = load_val::<L>(m_ctx, b, stack, offset);
    let val_as_i32 = b.build_sext(val, <i32>::get_type(m_ctx.llvm_ctx));

    stack.push(val_as_i32);
}

fn load_as_i32_zext<'a, L: Compile<'a>>(
    m_ctx: &'a ModuleCtx,
    b: &'a Builder,
    stack: &mut Vec<&'a Value>,
    offset: u32,
) {
    let val = load_val::<L>(m_ctx, b, stack, offset);
    let val_as_i32 = b.build_zext(val, <i32>::get_type(m_ctx.llvm_ctx));

    stack.push(val_as_i32);
}

fn load_as_i64_sext<'a, L: Compile<'a>>(
    m_ctx: &'a ModuleCtx,
    b: &'a Builder,
    stack: &mut Vec<&'a Value>,
    offset: u32,
) {
    let val = load_val::<L>(m_ctx, b, stack, offset);
    let val_as_i64 = b.build_sext(val, <i64>::get_type(m_ctx.llvm_ctx));
    stack.push(val_as_i64);
}

fn load_as_i64_zext<'a, L: Compile<'a>>(
    m_ctx: &'a ModuleCtx,
    b: &'a Builder,
    stack: &mut Vec<&'a Value>,
    offset: u32,
) {
    let val = load_val::<L>(m_ctx, b, stack, offset);
    let val_as_i64 = b.build_zext(val, <i64>::get_type(m_ctx.llvm_ctx));
    stack.push(val_as_i64);
}

fn store_val<'a, L: Compile<'a>>(
    m_ctx: &'a ModuleCtx,
    b: &'a Builder,
    stack: &mut Vec<&'a Value>,
    offset: u32,
    val: &Value,
) {
    let stack_offset = stack.pop().unwrap();
    let total_offset = b.build_add(stack_offset, offset.compile(m_ctx.llvm_ctx));

    let ty = L::get_type(m_ctx.llvm_ctx);
    let f = if ty == <i8>::get_type(m_ctx.llvm_ctx) {
        get_stub_function(m_ctx, SET_I8)
    } else if ty == <i16>::get_type(m_ctx.llvm_ctx) {
        get_stub_function(m_ctx, SET_I16)
    } else if ty == <i32>::get_type(m_ctx.llvm_ctx) {
        get_stub_function(m_ctx, SET_I32)
    } else if ty == <i64>::get_type(m_ctx.llvm_ctx) {
        get_stub_function(m_ctx, SET_I64)
    } else if ty == <f32>::get_type(m_ctx.llvm_ctx) {
        get_stub_function(m_ctx, SET_F32)
    } else if ty == <f64>::get_type(m_ctx.llvm_ctx) {
        get_stub_function(m_ctx, SET_F64)
    } else {
        panic!("cannot store value of type {:?}", ty)
    };

    b.build_call(f, &[total_offset, val]);
}
