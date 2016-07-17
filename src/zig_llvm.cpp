/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of zig, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

// This must go before all includes.
#include "config.h"
#if defined(ZIG_LLVM_OLD_CXX_ABI)
#define _GLIBCXX_USE_CXX11_ABI 0
#endif


#include "zig_llvm.hpp"


/*
 * The point of this file is to contain all the LLVM C++ API interaction so that:
 * 1. The compile time of other files is kept under control.
 * 2. Provide a C interface to the LLVM functions we need for self-hosting purposes.
 * 3. Prevent C++ from infecting the rest of the project.
 */

#include <llvm/InitializePasses.h>
#include <llvm/PassRegistry.h>
#include <llvm/MC/SubtargetFeature.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetParser.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DiagnosticInfo.h>
#include <llvm/Analysis/TargetLibraryInfo.h>
#include <llvm/Analysis/TargetTransformInfo.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/Transforms/Scalar.h>

using namespace llvm;

void ZigLLVMInitializeLoopStrengthReducePass(LLVMPassRegistryRef R) {
    initializeLoopStrengthReducePass(*unwrap(R));
}

void ZigLLVMInitializeLowerIntrinsicsPass(LLVMPassRegistryRef R) {
    initializeLowerIntrinsicsPass(*unwrap(R));
}

void ZigLLVMInitializeUnreachableBlockElimPass(LLVMPassRegistryRef R) {
    initializeUnreachableBlockElimPass(*unwrap(R));
}

char *ZigLLVMGetHostCPUName(void) {
    std::string str = sys::getHostCPUName();
    return strdup(str.c_str());
}

char *ZigLLVMGetNativeFeatures(void) {
    SubtargetFeatures features;

    StringMap<bool> host_features;
    if (sys::getHostCPUFeatures(host_features)) {
        for (auto &F : host_features)
            features.AddFeature(F.first(), F.second);
    }

    return strdup(features.getString().c_str());
}

static void addAddDiscriminatorsPass(const PassManagerBuilder &Builder, legacy::PassManagerBase &PM) {
  PM.add(createAddDiscriminatorsPass());
}


void ZigLLVMOptimizeModule(LLVMTargetMachineRef targ_machine_ref, LLVMModuleRef module_ref) {
    TargetMachine* target_machine = reinterpret_cast<TargetMachine*>(targ_machine_ref);
    Module* module = unwrap(module_ref);
    TargetLibraryInfoImpl tlii(Triple(module->getTargetTriple()));

    PassManagerBuilder *PMBuilder = new PassManagerBuilder();
    PMBuilder->OptLevel = target_machine->getOptLevel();
    PMBuilder->SizeLevel = 0;
    PMBuilder->BBVectorize = true;
    PMBuilder->SLPVectorize = true;
    PMBuilder->LoopVectorize = true;

    PMBuilder->DisableUnitAtATime = false;
    PMBuilder->DisableUnrollLoops = false;
    PMBuilder->MergeFunctions = true;
    PMBuilder->PrepareForLTO = true;
    PMBuilder->RerollLoops = true;

    PMBuilder->addExtension(PassManagerBuilder::EP_EarlyAsPossible, addAddDiscriminatorsPass);

    PMBuilder->LibraryInfo = &tlii;

    PMBuilder->Inliner = createFunctionInliningPass(PMBuilder->OptLevel, PMBuilder->SizeLevel);

    // Set up the per-function pass manager.
    legacy::FunctionPassManager *FPM = new legacy::FunctionPassManager(module);
    FPM->add(createTargetTransformInfoWrapperPass(target_machine->getTargetIRAnalysis()));
#ifndef NDEBUG
    bool verify_module = true;
#else
    bool verify_module = false;
#endif
    if (verify_module) {
        FPM->add(createVerifierPass());
    }
    PMBuilder->populateFunctionPassManager(*FPM);

    // Set up the per-module pass manager.
    legacy::PassManager *MPM = new legacy::PassManager();
    MPM->add(createTargetTransformInfoWrapperPass(target_machine->getTargetIRAnalysis()));

    PMBuilder->populateModulePassManager(*MPM);


    // run per function optimization passes
    FPM->doInitialization();
    for (Function &F : *module)
      if (!F.isDeclaration())
        FPM->run(F);
    FPM->doFinalization();

    // run per module optimization passes
    MPM->run(*module);
}

LLVMValueRef ZigLLVMBuildCall(LLVMBuilderRef B, LLVMValueRef Fn, LLVMValueRef *Args,
        unsigned NumArgs, unsigned CC, const char *Name)
{
    CallInst *call_inst = CallInst::Create(unwrap(Fn), makeArrayRef(unwrap(Args), NumArgs), Name);
    call_inst->setCallingConv(CC);
    return wrap(unwrap(B)->Insert(call_inst));
}

void ZigLLVMAddNonNullAttr(LLVMValueRef fn, unsigned i)
{
    assert( isa<Function>(unwrap(fn)) );

    Function *unwrapped_function = reinterpret_cast<Function*>(unwrap(fn));

    unwrapped_function->addAttribute(i, Attribute::NonNull);
}

void ZigLLVMFnSetSubprogram(LLVMValueRef fn, ZigLLVMDISubprogram *subprogram) {
    assert( isa<Function>(unwrap(fn)) );
    Function *unwrapped_function = reinterpret_cast<Function*>(unwrap(fn));
    unwrapped_function->setSubprogram(reinterpret_cast<DISubprogram*>(subprogram));
}


ZigLLVMDIType *ZigLLVMCreateDebugPointerType(ZigLLVMDIBuilder *dibuilder, ZigLLVMDIType *pointee_type,
        uint64_t size_in_bits, uint64_t align_in_bits, const char *name)
{
    DIType *di_type = reinterpret_cast<DIBuilder*>(dibuilder)->createPointerType(
            reinterpret_cast<DIType*>(pointee_type), size_in_bits, align_in_bits, name);
    return reinterpret_cast<ZigLLVMDIType*>(di_type);
}

ZigLLVMDIType *ZigLLVMCreateDebugBasicType(ZigLLVMDIBuilder *dibuilder, const char *name,
        uint64_t size_in_bits, uint64_t align_in_bits, unsigned encoding)
{
    DIType *di_type = reinterpret_cast<DIBuilder*>(dibuilder)->createBasicType(
            name, size_in_bits, align_in_bits, encoding);
    return reinterpret_cast<ZigLLVMDIType*>(di_type);
}

ZigLLVMDIType *ZigLLVMCreateDebugArrayType(ZigLLVMDIBuilder *dibuilder, uint64_t size_in_bits,
        uint64_t align_in_bits, ZigLLVMDIType *elem_type, int elem_count)
{
    SmallVector<Metadata *, 1> subrange;
    subrange.push_back(reinterpret_cast<DIBuilder*>(dibuilder)->getOrCreateSubrange(0, elem_count));
    DIType *di_type = reinterpret_cast<DIBuilder*>(dibuilder)->createArrayType(
            size_in_bits, align_in_bits,
            reinterpret_cast<DIType*>(elem_type),
            reinterpret_cast<DIBuilder*>(dibuilder)->getOrCreateArray(subrange));
    return reinterpret_cast<ZigLLVMDIType*>(di_type);
}

ZigLLVMDIEnumerator *ZigLLVMCreateDebugEnumerator(ZigLLVMDIBuilder *dibuilder, const char *name, int64_t val) {
    DIEnumerator *di_enumerator = reinterpret_cast<DIBuilder*>(dibuilder)->createEnumerator(name, val);
    return reinterpret_cast<ZigLLVMDIEnumerator*>(di_enumerator);
}

ZigLLVMDIType *ZigLLVMCreateDebugEnumerationType(ZigLLVMDIBuilder *dibuilder, ZigLLVMDIScope *scope,
        const char *name, ZigLLVMDIFile *file, unsigned line_number, uint64_t size_in_bits,
        uint64_t align_in_bits, ZigLLVMDIEnumerator **enumerator_array, int enumerator_array_len,
        ZigLLVMDIType *underlying_type, const char *unique_id)
{
    SmallVector<Metadata *, 8> fields;
    for (int i = 0; i < enumerator_array_len; i += 1) {
        DIEnumerator *dienumerator = reinterpret_cast<DIEnumerator*>(enumerator_array[i]);
        fields.push_back(dienumerator);
    }
    DIType *di_type = reinterpret_cast<DIBuilder*>(dibuilder)->createEnumerationType(
            reinterpret_cast<DIScope*>(scope),
            name,
            reinterpret_cast<DIFile*>(file),
            line_number, size_in_bits, align_in_bits,
            reinterpret_cast<DIBuilder*>(dibuilder)->getOrCreateArray(fields),
            reinterpret_cast<DIType*>(underlying_type),
            unique_id);
    return reinterpret_cast<ZigLLVMDIType*>(di_type);
}

ZigLLVMDIType *ZigLLVMCreateDebugMemberType(ZigLLVMDIBuilder *dibuilder, ZigLLVMDIScope *scope,
        const char *name, ZigLLVMDIFile *file, unsigned line, uint64_t size_in_bits,
        uint64_t align_in_bits, uint64_t offset_in_bits, unsigned flags, ZigLLVMDIType *type)
{
    DIType *di_type = reinterpret_cast<DIBuilder*>(dibuilder)->createMemberType(
            reinterpret_cast<DIScope*>(scope),
            name,
            reinterpret_cast<DIFile*>(file),
            line, size_in_bits, align_in_bits, offset_in_bits, flags,
            reinterpret_cast<DIType*>(type));
    return reinterpret_cast<ZigLLVMDIType*>(di_type);
}

ZigLLVMDIType *ZigLLVMCreateDebugUnionType(ZigLLVMDIBuilder *dibuilder, ZigLLVMDIScope *scope,
        const char *name, ZigLLVMDIFile *file, unsigned line_number, uint64_t size_in_bits,
        uint64_t align_in_bits, unsigned flags, ZigLLVMDIType **types_array, int types_array_len,
        unsigned run_time_lang, const char *unique_id)
{
    SmallVector<Metadata *, 8> fields;
    for (int i = 0; i < types_array_len; i += 1) {
        DIType *ditype = reinterpret_cast<DIType*>(types_array[i]);
        fields.push_back(ditype);
    }
    DIType *di_type = reinterpret_cast<DIBuilder*>(dibuilder)->createUnionType(
            reinterpret_cast<DIScope*>(scope),
            name,
            reinterpret_cast<DIFile*>(file),
            line_number, size_in_bits, align_in_bits, flags,
            reinterpret_cast<DIBuilder*>(dibuilder)->getOrCreateArray(fields),
            run_time_lang, unique_id);
    return reinterpret_cast<ZigLLVMDIType*>(di_type);
}

ZigLLVMDIType *ZigLLVMCreateDebugStructType(ZigLLVMDIBuilder *dibuilder, ZigLLVMDIScope *scope,
        const char *name, ZigLLVMDIFile *file, unsigned line_number, uint64_t size_in_bits,
        uint64_t align_in_bits, unsigned flags, ZigLLVMDIType *derived_from, 
        ZigLLVMDIType **types_array, int types_array_len, unsigned run_time_lang, ZigLLVMDIType *vtable_holder,
        const char *unique_id)
{
    SmallVector<Metadata *, 8> fields;
    for (int i = 0; i < types_array_len; i += 1) {
        DIType *ditype = reinterpret_cast<DIType*>(types_array[i]);
        fields.push_back(ditype);
    }
    DIType *di_type = reinterpret_cast<DIBuilder*>(dibuilder)->createStructType(
            reinterpret_cast<DIScope*>(scope),
            name,
            reinterpret_cast<DIFile*>(file),
            line_number, size_in_bits, align_in_bits, flags,
            reinterpret_cast<DIType*>(derived_from),
            reinterpret_cast<DIBuilder*>(dibuilder)->getOrCreateArray(fields),
            run_time_lang,
            reinterpret_cast<DIType*>(vtable_holder),
            unique_id);
    return reinterpret_cast<ZigLLVMDIType*>(di_type);
}

ZigLLVMDIType *ZigLLVMCreateReplaceableCompositeType(ZigLLVMDIBuilder *dibuilder, unsigned tag,
        const char *name, ZigLLVMDIScope *scope, ZigLLVMDIFile *file, unsigned line)
{
    DIType *di_type = reinterpret_cast<DIBuilder*>(dibuilder)->createReplaceableCompositeType(
            tag, name,
            reinterpret_cast<DIScope*>(scope),
            reinterpret_cast<DIFile*>(file),
            line);
    return reinterpret_cast<ZigLLVMDIType*>(di_type);
}

ZigLLVMDIType *ZigLLVMCreateDebugForwardDeclType(ZigLLVMDIBuilder *dibuilder, unsigned tag,
        const char *name, ZigLLVMDIScope *scope, ZigLLVMDIFile *file, unsigned line)
{
    DIType *di_type = reinterpret_cast<DIBuilder*>(dibuilder)->createForwardDecl(
            tag, name,
            reinterpret_cast<DIScope*>(scope),
            reinterpret_cast<DIFile*>(file),
            line);
    return reinterpret_cast<ZigLLVMDIType*>(di_type);
}

void ZigLLVMReplaceTemporary(ZigLLVMDIBuilder *dibuilder, ZigLLVMDIType *type,
        ZigLLVMDIType *replacement)
{
    reinterpret_cast<DIBuilder*>(dibuilder)->replaceTemporary(
            TempDIType(reinterpret_cast<DIType*>(type)),
            reinterpret_cast<DIType*>(replacement));
}

void ZigLLVMReplaceDebugArrays(ZigLLVMDIBuilder *dibuilder, ZigLLVMDIType *type,
        ZigLLVMDIType **types_array, int types_array_len)
{
    SmallVector<Metadata *, 8> fields;
    for (int i = 0; i < types_array_len; i += 1) {
        DIType *ditype = reinterpret_cast<DIType*>(types_array[i]);
        fields.push_back(ditype);
    }
    DICompositeType *composite_type = (DICompositeType*)reinterpret_cast<DIType*>(type);
    reinterpret_cast<DIBuilder*>(dibuilder)->replaceArrays(
            composite_type,
            reinterpret_cast<DIBuilder*>(dibuilder)->getOrCreateArray(fields));
}

ZigLLVMDIType *ZigLLVMCreateSubroutineType(ZigLLVMDIBuilder *dibuilder_wrapped,
        ZigLLVMDIType **types_array, int types_array_len, unsigned flags)
{
    SmallVector<Metadata *, 8> types;
    for (int i = 0; i < types_array_len; i += 1) {
        DIType *ditype = reinterpret_cast<DIType*>(types_array[i]);
        types.push_back(ditype);
    }
    DIBuilder *dibuilder = reinterpret_cast<DIBuilder*>(dibuilder_wrapped);
    DISubroutineType *subroutine_type = dibuilder->createSubroutineType(
            dibuilder->getOrCreateTypeArray(types),
            flags);
    DIType *ditype = subroutine_type;
    return reinterpret_cast<ZigLLVMDIType*>(ditype);
}


ZigLLVMDIType *ZigLLVMCreateTypedef(ZigLLVMDIBuilder *dibuilder, ZigLLVMDIType *ty, const char *name,
        ZigLLVMDIFile *file, unsigned line, ZigLLVMDIScope *scope)
{
    DIDerivedType *derived_type = reinterpret_cast<DIBuilder*>(dibuilder)->createTypedef(
            reinterpret_cast<DIType*>(ty),
            name,
            reinterpret_cast<DIFile*>(file),
            line,
            reinterpret_cast<DIScope*>(scope));
    DIType *ditype = derived_type;
    return reinterpret_cast<ZigLLVMDIType*>(ditype);
}

unsigned ZigLLVMEncoding_DW_ATE_unsigned(void) {
    return dwarf::DW_ATE_unsigned;
}

unsigned ZigLLVMEncoding_DW_ATE_signed(void) {
    return dwarf::DW_ATE_signed;
}

unsigned ZigLLVMEncoding_DW_ATE_float(void) {
    return dwarf::DW_ATE_float;
}

unsigned ZigLLVMEncoding_DW_ATE_boolean(void) {
    return dwarf::DW_ATE_boolean;
}

unsigned ZigLLVMEncoding_DW_ATE_unsigned_char(void) {
    return dwarf::DW_ATE_unsigned_char;
}

unsigned ZigLLVMEncoding_DW_ATE_signed_char(void) {
    return dwarf::DW_ATE_signed_char;
}

unsigned ZigLLVMLang_DW_LANG_C99(void) {
    return dwarf::DW_LANG_C99;
}

unsigned ZigLLVMTag_DW_variable(void) {
    return dwarf::DW_TAG_variable;
}

unsigned ZigLLVMTag_DW_structure_type(void) {
    return dwarf::DW_TAG_structure_type;
}

ZigLLVMDIBuilder *ZigLLVMCreateDIBuilder(LLVMModuleRef module, bool allow_unresolved) {
    DIBuilder *di_builder = new DIBuilder(*unwrap(module), allow_unresolved);
    return reinterpret_cast<ZigLLVMDIBuilder *>(di_builder);
}

void ZigLLVMSetCurrentDebugLocation(LLVMBuilderRef builder, int line, int column, ZigLLVMDIScope *scope) {
    unwrap(builder)->SetCurrentDebugLocation(DebugLoc::get(
                line, column, reinterpret_cast<DIScope*>(scope)));
}

void ZigLLVMClearCurrentDebugLocation(LLVMBuilderRef builder) {
    unwrap(builder)->SetCurrentDebugLocation(DebugLoc());
}


ZigLLVMDILexicalBlock *ZigLLVMCreateLexicalBlock(ZigLLVMDIBuilder *dbuilder, ZigLLVMDIScope *scope,
        ZigLLVMDIFile *file, unsigned line, unsigned col)
{
    DILexicalBlock *result = reinterpret_cast<DIBuilder*>(dbuilder)->createLexicalBlock(
            reinterpret_cast<DIScope*>(scope),
            reinterpret_cast<DIFile*>(file),
            line,
            col);
    return reinterpret_cast<ZigLLVMDILexicalBlock*>(result);
}

ZigLLVMDILocalVariable *ZigLLVMCreateAutoVariable(ZigLLVMDIBuilder *dbuilder,
        ZigLLVMDIScope *scope, const char *name, ZigLLVMDIFile *file, unsigned line_no,
        ZigLLVMDIType *type, bool always_preserve, unsigned flags)
{
    DILocalVariable *result = reinterpret_cast<DIBuilder*>(dbuilder)->createAutoVariable(
            reinterpret_cast<DIScope*>(scope),
            name,
            reinterpret_cast<DIFile*>(file),
            line_no,
            reinterpret_cast<DIType*>(type),
            always_preserve,
            flags);
    return reinterpret_cast<ZigLLVMDILocalVariable*>(result);
}

ZigLLVMDILocalVariable *ZigLLVMCreateParameterVariable(ZigLLVMDIBuilder *dbuilder,
        ZigLLVMDIScope *scope, const char *name, ZigLLVMDIFile *file, unsigned line_no,
        ZigLLVMDIType *type, bool always_preserve, unsigned flags, unsigned arg_no)
{
    DILocalVariable *result = reinterpret_cast<DIBuilder*>(dbuilder)->createParameterVariable(
            reinterpret_cast<DIScope*>(scope),
            name,
            arg_no,
            reinterpret_cast<DIFile*>(file),
            line_no,
            reinterpret_cast<DIType*>(type),
            always_preserve,
            flags);
    return reinterpret_cast<ZigLLVMDILocalVariable*>(result);
}

ZigLLVMDIScope *ZigLLVMLexicalBlockToScope(ZigLLVMDILexicalBlock *lexical_block) {
    DIScope *scope = reinterpret_cast<DILexicalBlock*>(lexical_block);
    return reinterpret_cast<ZigLLVMDIScope*>(scope);
}

ZigLLVMDIScope *ZigLLVMCompileUnitToScope(ZigLLVMDICompileUnit *compile_unit) {
    DIScope *scope = reinterpret_cast<DICompileUnit*>(compile_unit);
    return reinterpret_cast<ZigLLVMDIScope*>(scope);
}

ZigLLVMDIScope *ZigLLVMFileToScope(ZigLLVMDIFile *difile) {
    DIScope *scope = reinterpret_cast<DIFile*>(difile);
    return reinterpret_cast<ZigLLVMDIScope*>(scope);
}

ZigLLVMDIScope *ZigLLVMSubprogramToScope(ZigLLVMDISubprogram *subprogram) {
    DIScope *scope = reinterpret_cast<DISubprogram*>(subprogram);
    return reinterpret_cast<ZigLLVMDIScope*>(scope);
}

ZigLLVMDIScope *ZigLLVMTypeToScope(ZigLLVMDIType *type) {
    DIScope *scope = reinterpret_cast<DIType*>(type);
    return reinterpret_cast<ZigLLVMDIScope*>(scope);
}

ZigLLVMDICompileUnit *ZigLLVMCreateCompileUnit(ZigLLVMDIBuilder *dibuilder,
        unsigned lang, const char *file, const char *dir, const char *producer,
        bool is_optimized, const char *flags, unsigned runtime_version, const char *split_name,
        uint64_t dwo_id, bool emit_debug_info)
{
    DICompileUnit *result = reinterpret_cast<DIBuilder*>(dibuilder)->createCompileUnit(
            lang, file, dir, producer, is_optimized, flags, runtime_version, split_name,
            DIBuilder::FullDebug, dwo_id, emit_debug_info);
    return reinterpret_cast<ZigLLVMDICompileUnit*>(result);
}

ZigLLVMDIFile *ZigLLVMCreateFile(ZigLLVMDIBuilder *dibuilder, const char *filename, const char *directory) {
    DIFile *result = reinterpret_cast<DIBuilder*>(dibuilder)->createFile(filename, directory);
    return reinterpret_cast<ZigLLVMDIFile*>(result);
}

ZigLLVMDISubprogram *ZigLLVMCreateFunction(ZigLLVMDIBuilder *dibuilder, ZigLLVMDIScope *scope,
        const char *name, const char *linkage_name, ZigLLVMDIFile *file, unsigned lineno,
        ZigLLVMDIType *fn_di_type, bool is_local_to_unit, bool is_definition, unsigned scope_line,
        unsigned flags, bool is_optimized, ZigLLVMDISubprogram *decl_subprogram)
{
    DISubroutineType *di_sub_type = static_cast<DISubroutineType*>(reinterpret_cast<DIType*>(fn_di_type));
    DISubprogram *result = reinterpret_cast<DIBuilder*>(dibuilder)->createFunction(
            reinterpret_cast<DIScope*>(scope),
            name, linkage_name,
            reinterpret_cast<DIFile*>(file),
            lineno,
            di_sub_type,
            is_local_to_unit, is_definition, scope_line, flags, is_optimized,
            nullptr,
            reinterpret_cast<DISubprogram *>(decl_subprogram));
    return reinterpret_cast<ZigLLVMDISubprogram*>(result);
}

void ZigLLVMDIBuilderFinalize(ZigLLVMDIBuilder *dibuilder) {
    reinterpret_cast<DIBuilder*>(dibuilder)->finalize();
}

ZigLLVMInsertionPoint *ZigLLVMSaveInsertPoint(LLVMBuilderRef builder_wrapped) {
    IRBuilderBase::InsertPoint *ip = new IRBuilderBase::InsertPoint();
    *ip = unwrap(builder_wrapped)->saveIP();
    return reinterpret_cast<ZigLLVMInsertionPoint*>(ip);
}

void ZigLLVMRestoreInsertPoint(LLVMBuilderRef builder, ZigLLVMInsertionPoint *ip_wrapped) {
    IRBuilderBase::InsertPoint *ip = reinterpret_cast<IRBuilderBase::InsertPoint*>(ip_wrapped);
    unwrap(builder)->restoreIP(*ip);
}

LLVMValueRef ZigLLVMInsertDeclareAtEnd(ZigLLVMDIBuilder *dibuilder, LLVMValueRef storage,
        ZigLLVMDILocalVariable *var_info, ZigLLVMDILocation *debug_loc, LLVMBasicBlockRef basic_block_ref)
{
    Instruction *result = reinterpret_cast<DIBuilder*>(dibuilder)->insertDeclare(
            unwrap(storage),
            reinterpret_cast<DILocalVariable *>(var_info),
            reinterpret_cast<DIBuilder*>(dibuilder)->createExpression(),
            reinterpret_cast<DILocation*>(debug_loc),
            static_cast<BasicBlock*>(unwrap(basic_block_ref)));
    return wrap(result);
}

LLVMValueRef ZigLLVMInsertDeclare(ZigLLVMDIBuilder *dibuilder, LLVMValueRef storage,
        ZigLLVMDILocalVariable *var_info, ZigLLVMDILocation *debug_loc, LLVMValueRef insert_before_instr)
{
    Instruction *result = reinterpret_cast<DIBuilder*>(dibuilder)->insertDeclare(
            unwrap(storage),
            reinterpret_cast<DILocalVariable *>(var_info),
            reinterpret_cast<DIBuilder*>(dibuilder)->createExpression(),
            reinterpret_cast<DILocation*>(debug_loc),
            static_cast<Instruction*>(unwrap(insert_before_instr)));
    return wrap(result);
}

ZigLLVMDILocation *ZigLLVMGetDebugLoc(unsigned line, unsigned col, ZigLLVMDIScope *scope) {
    DebugLoc debug_loc = DebugLoc::get(line, col, reinterpret_cast<DIScope*>(scope), nullptr);
    return reinterpret_cast<ZigLLVMDILocation*>(debug_loc.get());
}

void ZigLLVMSetFastMath(LLVMBuilderRef builder_wrapped, bool on_state) {
    if (on_state) {
        FastMathFlags fmf;
        fmf.setUnsafeAlgebra();
        unwrap(builder_wrapped)->setFastMathFlags(fmf);
    } else {
        unwrap(builder_wrapped)->clearFastMathFlags();
    }
}

void ZigLLVMAddFunctionAttr(LLVMValueRef fn_ref, const char *attr_name, const char *attr_value) {
    Function *func = unwrap<Function>(fn_ref);
    const AttributeSet attr_set = func->getAttributes();
    AttrBuilder attr_builder;
    if (attr_value) {
        attr_builder.addAttribute(attr_name, attr_value);
    } else {
        attr_builder.addAttribute(attr_name);
    }
    const AttributeSet new_attr_set = attr_set.addAttributes(func->getContext(),
            AttributeSet::FunctionIndex, AttributeSet::get(func->getContext(),
                AttributeSet::FunctionIndex, attr_builder));
    func->setAttributes(new_attr_set);
}


static_assert((Triple::ArchType)ZigLLVM_LastArchType == Triple::LastArchType, "");
static_assert((Triple::VendorType)ZigLLVM_LastVendorType == Triple::LastVendorType, "");
static_assert((Triple::OSType)ZigLLVM_LastOSType == Triple::LastOSType, "");
static_assert((Triple::EnvironmentType)ZigLLVM_LastEnvironmentType == Triple::LastEnvironmentType, "");

static_assert((Triple::ObjectFormatType)ZigLLVM_UnknownObjectFormat == Triple::UnknownObjectFormat, "");
static_assert((Triple::ObjectFormatType)ZigLLVM_COFF == Triple::COFF, "");
static_assert((Triple::ObjectFormatType)ZigLLVM_ELF == Triple::ELF, "");
static_assert((Triple::ObjectFormatType)ZigLLVM_MachO == Triple::MachO, "");

const char *ZigLLVMGetArchTypeName(ZigLLVM_ArchType arch) {
    return Triple::getArchTypeName((Triple::ArchType)arch);
}

const char *ZigLLVMGetVendorTypeName(ZigLLVM_VendorType vendor) {
    return Triple::getVendorTypeName((Triple::VendorType)vendor);
}

const char *ZigLLVMGetOSTypeName(ZigLLVM_OSType os) {
    return Triple::getOSTypeName((Triple::OSType)os);
}

const char *ZigLLVMGetEnvironmentTypeName(ZigLLVM_EnvironmentType env_type) {
    return Triple::getEnvironmentTypeName((Triple::EnvironmentType)env_type);
}

void ZigLLVMGetNativeTarget(ZigLLVM_ArchType *arch_type, ZigLLVM_SubArchType *sub_arch_type,
        ZigLLVM_VendorType *vendor_type, ZigLLVM_OSType *os_type, ZigLLVM_EnvironmentType *environ_type,
        ZigLLVM_ObjectFormatType *oformat)
{
    char *native_triple = LLVMGetDefaultTargetTriple();
    Triple triple(native_triple);

    *arch_type = (ZigLLVM_ArchType)triple.getArch();
    *sub_arch_type = (ZigLLVM_SubArchType)triple.getSubArch();
    *vendor_type = (ZigLLVM_VendorType)triple.getVendor();
    *os_type = (ZigLLVM_OSType)triple.getOS();
    *environ_type = (ZigLLVM_EnvironmentType)triple.getEnvironment();
    *oformat = (ZigLLVM_ObjectFormatType)triple.getObjectFormat();

    free(native_triple);
}

const char *ZigLLVMGetSubArchTypeName(ZigLLVM_SubArchType sub_arch) {
    switch (sub_arch) {
        case ZigLLVM_NoSubArch:
            return "(none)";
        case ZigLLVM_ARMSubArch_v8_2a:
            return "v8_2a";
        case ZigLLVM_ARMSubArch_v8_1a:
            return "v8_1a";
        case ZigLLVM_ARMSubArch_v8:
            return "v8";
        case ZigLLVM_ARMSubArch_v7:
            return "v7";
        case ZigLLVM_ARMSubArch_v7em:
            return "v7em";
        case ZigLLVM_ARMSubArch_v7m:
            return "v7m";
        case ZigLLVM_ARMSubArch_v7s:
            return "v7s";
        case ZigLLVM_ARMSubArch_v7k:
            return "v7k";
        case ZigLLVM_ARMSubArch_v6:
            return "v6";
        case ZigLLVM_ARMSubArch_v6m:
            return "v6m";
        case ZigLLVM_ARMSubArch_v6k:
            return "v6k";
        case ZigLLVM_ARMSubArch_v6t2:
            return "v6t2";
        case ZigLLVM_ARMSubArch_v5:
            return "v5";
        case ZigLLVM_ARMSubArch_v5te:
            return "v5te";
        case ZigLLVM_ARMSubArch_v4t:
            return "v4t";
        case ZigLLVM_KalimbaSubArch_v3:
            return "v3";
        case ZigLLVM_KalimbaSubArch_v4:
            return "v4";
        case ZigLLVM_KalimbaSubArch_v5:
            return "v5";
    }
    abort();
}

void ZigLLVMAddModuleDebugInfoFlag(LLVMModuleRef module) {
    unwrap(module)->addModuleFlag(Module::Warning, "Debug Info Version", DEBUG_METADATA_VERSION);
}

unsigned ZigLLVMGetPrefTypeAlignment(LLVMTargetDataRef TD, LLVMTypeRef Ty) {
    return unwrap(TD)->getPrefTypeAlignment(unwrap(Ty));
}


static AtomicOrdering mapFromLLVMOrdering(LLVMAtomicOrdering Ordering) {
    switch (Ordering) {
        case LLVMAtomicOrderingNotAtomic: return NotAtomic;
        case LLVMAtomicOrderingUnordered: return Unordered;
        case LLVMAtomicOrderingMonotonic: return Monotonic;
        case LLVMAtomicOrderingAcquire: return Acquire;
        case LLVMAtomicOrderingRelease: return Release;
        case LLVMAtomicOrderingAcquireRelease: return AcquireRelease;
        case LLVMAtomicOrderingSequentiallyConsistent: return SequentiallyConsistent;
    }
    abort();
}

LLVMValueRef ZigLLVMBuildCmpXchg(LLVMBuilderRef builder, LLVMValueRef ptr, LLVMValueRef cmp,
        LLVMValueRef new_val, LLVMAtomicOrdering success_ordering,
        LLVMAtomicOrdering failure_ordering)
{
    return wrap(unwrap(builder)->CreateAtomicCmpXchg(unwrap(ptr), unwrap(cmp), unwrap(new_val),
                mapFromLLVMOrdering(success_ordering), mapFromLLVMOrdering(failure_ordering),
                CrossThread));
}

LLVMValueRef ZigLLVMBuildNSWShl(LLVMBuilderRef builder, LLVMValueRef LHS, LLVMValueRef RHS,
        const char *name)
{
    return wrap(unwrap(builder)->CreateShl(unwrap(LHS), unwrap(RHS), name, false, true));
}

LLVMValueRef ZigLLVMBuildNUWShl(LLVMBuilderRef builder, LLVMValueRef LHS, LLVMValueRef RHS,
        const char *name)
{
    return wrap(unwrap(builder)->CreateShl(unwrap(LHS), unwrap(RHS), name, false, true));
}

LLVMValueRef ZigLLVMBuildExactUDiv(LLVMBuilderRef B, LLVMValueRef LHS,
        LLVMValueRef RHS, const char *Name)
{
    return wrap(unwrap(B)->CreateExactUDiv(unwrap(LHS), unwrap(RHS), Name));
}



//------------------------------------

#include "buffer.hpp"

void ZigLLVMGetTargetTriple(Buf *out_buf, ZigLLVM_ArchType arch_type, ZigLLVM_SubArchType sub_arch_type,
        ZigLLVM_VendorType vendor_type, ZigLLVM_OSType os_type, ZigLLVM_EnvironmentType environ_type,
        ZigLLVM_ObjectFormatType oformat)
{
    Triple triple;

    triple.setArch((Triple::ArchType)arch_type);
    // TODO how to set the sub arch?
    triple.setVendor((Triple::VendorType)vendor_type);
    triple.setOS((Triple::OSType)os_type);
    triple.setEnvironment((Triple::EnvironmentType)environ_type);
    // I guess it's a "triple" because we don't set the object format?
    //triple.setObjectFormat((Triple::ObjectFormatType)oformat);

    const std::string &str = triple.str();
    buf_init_from_mem(out_buf, str.c_str(), str.size());
}

enum FloatAbi {
    FloatAbiHard,
    FloatAbiSoft,
    FloatAbiSoftFp,
};

static FloatAbi get_float_abi(const Triple &triple) {
    zig_panic("TODO implement get_float_abi for ARM");
}

Buf *get_dynamic_linker(LLVMTargetMachineRef target_machine_ref) {
    TargetMachine *target_machine = reinterpret_cast<TargetMachine*>(target_machine_ref);
    const Triple &triple = target_machine->getTargetTriple();

    const Triple::ArchType arch = triple.getArch();

    if (triple.getEnvironment() == Triple::Android) {
        if (triple.isArch64Bit()) {
            return buf_create_from_str("/system/bin/linker64");
        } else {
            return buf_create_from_str("/system/bin/linker");
        }
    } else if (arch == Triple::x86 ||
            arch == Triple::sparc ||
            arch == Triple::sparcel)
    {
        return buf_create_from_str("/lib/ld-linux.so.2");
    } else if (arch == Triple::aarch64) {
        return buf_create_from_str("/lib/ld-linux-aarch64.so.1");
    } else if (arch == Triple::aarch64_be) {
        return buf_create_from_str("/lib/ld-linux-aarch64_be.so.1");
    } else if (arch == Triple::arm || arch == Triple::thumb) {
        if (triple.getEnvironment() == Triple::GNUEABIHF ||
            get_float_abi(triple) == FloatAbiHard)
        {
            return buf_create_from_str("/lib/ld-linux-armhf.so.3");
        } else {
            return buf_create_from_str("/lib/ld-linux.so.3");
        }
    } else if (arch == Triple::armeb || arch == Triple::thumbeb) {
        if (triple.getEnvironment() == Triple::GNUEABIHF ||
            get_float_abi(triple) == FloatAbiHard)
        {
            return buf_create_from_str("/lib/ld-linux-armhf.so.3");
        } else {
            return buf_create_from_str("/lib/ld-linux.so.3");
        }
    } else if (arch == Triple::mips || arch == Triple::mipsel ||
            arch == Triple::mips64 || arch == Triple::mips64el)
    {
        // when you want to solve this TODO, grep clang codebase for
        // getLinuxDynamicLinker
        zig_panic("TODO figure out MIPS dynamic linker name");
    } else if (arch == Triple::ppc) {
        return buf_create_from_str("/lib/ld.so.1");
    } else if (arch == Triple::ppc64) {
        return buf_create_from_str("/lib64/ld64.so.2");
    } else if (arch == Triple::ppc64le) {
        return buf_create_from_str("/lib64/ld64.so.2");
    } else if (arch == Triple::systemz) {
        return buf_create_from_str("/lib64/ld64.so.1");
    } else if (arch == Triple::sparcv9) {
        return buf_create_from_str("/lib64/ld-linux.so.2");
    } else if (arch == Triple::x86_64 &&
            triple.getEnvironment() == Triple::GNUX32)
    {
        return buf_create_from_str("/libx32/ld-linux-x32.so.2");
    } else {
        return buf_create_from_str("/lib64/ld-linux-x86-64.so.2");
    }
}

