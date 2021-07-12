#![allow(non_upper_case_globals)]

use std::os::raw::c_char;
use std::os::raw::c_uint;

use llvm::ffi::LLVMAttribute;
use llvm::ffi::LLVMContext;
use llvm::ffi::LLVMValue;

// WARNING: This is a bit of hack, and we can hopefully replace llvm_alt with `inkwell` at some point
// We already link LLVM thru llvm_alt, but it fails to pick up a few functions we need

pub const LLVMAttributeFunctionIndex: c_uint = !0;

pub type LLVMAttributeRef = *mut LLVMAttribute;

extern "C" {
    pub fn LLVMAddAttributeAtIndex(F: *mut LLVMValue, Idx: u32, A: LLVMAttributeRef);

    pub fn LLVMCreateEnumAttribute(
        C: *mut LLVMContext,
        KindID: c_uint,
        Val: u64,
    ) -> LLVMAttributeRef;

    pub fn LLVMCreateStringAttribute(
        C: *mut LLVMContext,
        K: *const c_char,
        KLength: c_uint,
        V: *const c_char,
        VLength: c_uint,
    ) -> LLVMAttributeRef;

    pub fn LLVMGetEnumAttributeKindForName(
        Name: *const c_char,
        SLen: usize,
    ) -> c_uint;
}
