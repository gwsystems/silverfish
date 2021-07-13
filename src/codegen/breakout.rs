use std::cell::RefCell;
use std::rc::Rc;

use llvm::BasicBlock;
use llvm::Builder;
use llvm::Context;
use llvm::Value;

use wasmparser::TypeOrFuncType;

use crate::codegen::type_conversions::llvm_type_to_zeroed_value;
use crate::codegen::type_conversions::wasm_type_to_zeroed_value;

pub struct BreakoutTarget<'a> {
    pub bb: &'a BasicBlock,
    result_type: Option<TypeOrFuncType>,
    jumps: Vec<JumpFrom<'a>>,
}

pub struct JumpFrom<'a> {
    from: &'a BasicBlock,
    result: Option<&'a Value>,
    locals: Vec<&'a Value>,
}

pub type WBreakoutTarget<'a> = Rc<RefCell<BreakoutTarget<'a>>>;

impl<'a> BreakoutTarget<'a> {
    pub fn new(bb: &'a BasicBlock, result_type: Option<TypeOrFuncType>) -> BreakoutTarget<'a> {
        BreakoutTarget {
            bb,
            result_type,
            jumps: Vec::new(),
        }
    }

    pub fn new_wrapped(
        bb: &'a BasicBlock,
        result_type: Option<TypeOrFuncType>,
    ) -> WBreakoutTarget<'a> {
        let bt = Self::new(bb, result_type);
        Rc::new(RefCell::new(bt))
    }

    pub fn build_locals<'b>(
        &self,
        ctx: &'b Context,
        b: &'b Builder,
        old_locals: &[&'b Value],
    ) -> Vec<&'b Value>
    where
        'a: 'b,
    {
        let local_count = old_locals.len();

        let mut jump_back_values: Vec<Vec<(&BasicBlock, &Value)>> =
            old_locals.iter().map(|_| Vec::new()).collect();
        for jf in &self.jumps {
            for (i, &l) in jf.locals.iter().enumerate() {
                jump_back_values[i].push((jf.from, l))
            }
        }

        let mut new_locals = Vec::new();
        for i in 0..local_count {
            let local_type = old_locals[i].get_type();
            let jump_back_count = jump_back_values[i].len();

            if jump_back_count == 0 {
                // Sometimes if a block doesn't terminate there might not be a jump back
                // We probably should emit an unreachable instruction in this case, but that might mess up future compilation
                // Instead we just produce zeroes, and hope llvm notices this is unreachable code
                // TODO: Figure out if we can instead produce an unreachable result
                new_locals.push(llvm_type_to_zeroed_value(ctx, local_type));
            } else if jump_back_count == 1 {
                new_locals.push(jump_back_values[i][0].1);
            } else if !jump_back_values[i].is_empty() {
                new_locals.push(b.build_phi(jump_back_values[i].clone()));
            }
        }

        new_locals
    }

    pub fn modify_phis<'b>(&self, phi_instructions: &[&'b Value])
    where
        'a: 'b,
    {
        let phi_count = phi_instructions.len();

        let mut jump_back_values: Vec<Vec<(&BasicBlock, &Value)>> =
            phi_instructions.iter().map(|_| Vec::new()).collect();
        for jf in &self.jumps {
            for i in 0..phi_count {
                let l = jf.locals[i];
                jump_back_values[i].push((jf.from, l))
            }
        }

        for i in 0..phi_count {
            modify_phi_multi(phi_instructions[i], jump_back_values[i].clone());
        }
    }

    pub fn build_result<'b>(&self, ctx: &'b Context, b: &'b Builder) -> Option<&'b Value>
    where
        'a: 'b,
    {
        self.result_type.map(|ty| {
            let mut vals: Vec<(&BasicBlock, &Value)> = Vec::new();
            for jf in &self.jumps {
                vals.push((jf.from, jf.result.unwrap()))
            }

            if vals.is_empty() {
                // See above comment about unreachablity
                // TODO: Figure out if we can instead produce an unreachable result
                if let TypeOrFuncType::Type(t) = ty {
                    wasm_type_to_zeroed_value(ctx, t)
                } else {
                    panic!()
                }
            } else if vals.len() == 1 {
                vals[0].1
            } else {
                b.build_phi(vals)
            }
        })
    }

    pub fn add_jump<'b: 'a>(
        &mut self,
        from: &'a BasicBlock,
        locals: &[&'b Value],
        stack: &[&'b Value],
    ) {
        let res = if self.result_type.is_some() {
            Some(
                *stack
                    .last()
                    .expect("can't make a value jump without a stack value"),
            )
        } else {
            None
        };

        let jf = JumpFrom {
            from,
            result: res,
            locals: locals.into(),
        };

        self.jumps.push(jf);
    }
}

// This is kind of an oddball method, and a total hack
// But we need to modify phi instructions in this file, and this file only
// So I guess it lives here now...
pub fn modify_phi_multi(phi: &Value, values: Vec<(&BasicBlock, &Value)>) {
    for (bb, v) in values {
        modify_phi(phi, v, bb);
    }
}

pub fn modify_phi(phi: &Value, new_val: &Value, from: &BasicBlock) {
    use llvm::ffi::core;
    unsafe {
        core::LLVMAddIncoming(phi.into(), &mut new_val.into(), &mut from.into(), 1);
    }
}
