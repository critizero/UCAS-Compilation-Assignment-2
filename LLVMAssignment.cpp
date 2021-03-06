//===- Hello.cpp - Example code from "Writing an LLVM Pass" ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements two versions of the LLVM "Hello World" pass described
// in docs/WritingAnLLVMPass.html
//
//===----------------------------------------------------------------------===//

#include <llvm/Support/CommandLine.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/ToolOutputFile.h>

#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Utils.h>

#include <llvm/IR/Function.h>
#include <llvm/Pass.h>
#include <llvm/Support/raw_ostream.h>

#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Bitcode/BitcodeWriter.h>

#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Instructions.h>

#include <map>
#include <set>

using namespace llvm;
static ManagedStatic<LLVMContext> GlobalContext;
static LLVMContext &getGlobalContext() { return *GlobalContext; }
/* In LLVM 5.0, when  -O0 passed to clang , the functions generated with clang will
 * have optnone attribute which would lead to some transform passes disabled, like mem2reg.
 */
struct EnableFunctionOptPass: public FunctionPass {
    static char ID;
    EnableFunctionOptPass():FunctionPass(ID){}
    bool runOnFunction(Function & F) override{
        if(F.hasFnAttribute(Attribute::OptimizeNone))
        {
            F.removeFnAttr(Attribute::OptimizeNone);
        }
        return true;
    }
};

char EnableFunctionOptPass::ID=0;


///!TODO TO BE COMPLETED BY YOU FOR ASSIGNMENT 2
///Updated 11/10/2017 by fargo: make all functions
///processed by mem2reg before this pass.
struct FuncPtrPass : public ModulePass {
    static char ID; // Pass identification, replacement for typeid
    std::map<unsigned int, std::set<Function *>> mResult;

    FuncPtrPass() : ModulePass(ID), mResult() {}

    void calCallPtr(Function * func, Value * &func_ret,
                    std::map<Value *, std::set<Function *>> &phi_func,
                    std::map<Value *, Value *> &func_arg) {
        std::map<Value *, std::set<Value *>> sub_ret{};

        for (BasicBlock &BB : *func) {
            for (Instruction & I : BB) {
                if (auto *call = dyn_cast<CallBase>(&I)) {
//                    unsigned int debug_col = I.getDebugLoc().getLine();
//                    if (debug_col == 31 || debug_col == 25 || debug_col == 20) {
//                        I.dump();
//                    }
                    // ????????????
                    if (Function * callee = call->getCalledFunction()) {
                        // ???????????????
                        if (!callee->isIntrinsic()) {
                            //??????????????????
                            unsigned int col = I.getDebugLoc().getLine();
                            mResult[col].insert(callee);

                            // ????????????????????????
                            std::map<Value *, Value *> sub_arg{};   // ????????????
                            Value * sub_sub_ret = nullptr;  // ?????????
                            for (unsigned int i = 0, e = callee->arg_size(); i < e; ++i) { // ???????????????????????????
                                Value * opt = call->getOperand(i);
//                                callee->getArg(i)->dump();
                                if (func_arg.find(opt) != func_arg.end()) {
                                    // ???????????????????????????????????????????????????
//                                    func_arg[opt]->dump();
                                    sub_arg[callee->getArg(i)] = func_arg[opt];
                                }
                                else {
                                    // ???????????????????????????????????????
//                                    opt->dump();
                                    sub_arg[callee->getArg(i)] = opt;
                                }
                            }
                            // ????????????
                            calCallPtr(callee, sub_sub_ret, phi_func, sub_arg);
                            // ???????????????
                            if (sub_sub_ret != nullptr) {
//                                sub_sub_ret->dump();
//                                call->dump();
                                // ?????????????????????????????????
                                sub_ret[call].insert(sub_sub_ret);
                            }
                        }
                    }
                    // ????????????
                    else if (Value * call_ptr = call->getCalledOperand()) {
                        // ???????????????phi??????
                        if (auto phi = dyn_cast<PHINode>(call_ptr)) {
                            unsigned int col = I.getDebugLoc().getLine();
                            if (phi_func.count(phi) != 0) {
                                // ???????????????phi??????????????????
                                for (Function * candidate : phi_func[phi]) {
                                        mResult[col].insert(candidate);
                                }
                            }
                            else {
                                // ?????????????????????
                                for (unsigned int i = 0, e = phi->getNumIncomingValues(); i < e; ++i) {
                                    Value * opt = phi->getOperand(i);
                                    // ????????????????????????
                                    if (func_arg.find(opt) != func_arg.end()) {
                                        Value * sub_opt = func_arg[opt];
                                        unsigned int col = I.getDebugLoc().getLine();
                                        // ?????????
                                        if (auto sub_func = dyn_cast<Function>(sub_opt)) {
                                            mResult[col].insert(sub_func);
                                        }
                                        // ???phi??????
                                        else if (auto sub_phi = dyn_cast<PHINode>(sub_opt)) {
                                            if (phi_func.count(sub_phi) != 0) {
                                                for (Function * candidate : phi_func[sub_phi]) {
                                                    mResult[col].insert(candidate);
                                                }
                                            }
                                        }
                                    }
                                }
                            }
//                            errs() << "CAN ---\n";
//                            I.dump();
//                            phi->dump();
//                            call->dump();
//                            call_ptr->dump();
//                            errs() << "--- END\n";
                            // ????????????????????????
                            for (Function * sub_func : phi_func[phi]) {
                                std::map<Value *, Value *> sub_arg{};
                                Value * sub_sub_ret = nullptr;
                                for (unsigned int i = 0, e = sub_func->arg_size(); i < e; ++i) {
                                    Value * opt = call->getOperand(i);
                                    sub_arg[sub_func->getArg(i)] = opt;
//                                    sub_func->getArg(i)->dump();
//                                    opt->dump();
                                }
                                calCallPtr(sub_func, sub_sub_ret, phi_func, sub_arg);
                                if (sub_sub_ret != nullptr) {
//                                    sub_sub_ret->dump();
                                    sub_ret[call].insert(sub_sub_ret);
                                }
                            }
                        }
                        // ??????????????????????????????
                        else if (func_arg.find(call_ptr) != func_arg.end()) {
                            // ?????????phi??????
                            if (auto phi = dyn_cast<PHINode>(func_arg[call_ptr])) {
                                if (phi_func.count(phi) != 0) {
                                    unsigned int col = I.getDebugLoc().getLine();
                                    for (Function * candidate : phi_func[phi]) {
                                        mResult[col].insert(candidate);
                                    }
                                }
                            }
                            // ?????????
                            else if (auto candidate = dyn_cast<Function>(func_arg[call_ptr])) {
                                unsigned int col = I.getDebugLoc().getLine();
                                mResult[col].insert(candidate);

                                // ????????????
                                std::map<Value *, Value *> sub_arg{};
                                Value * sub_sub_ret = nullptr;
                                for (unsigned int i = 0, e = candidate->arg_size(); i < e; ++i) {
                                    Value * opt = call->getOperand(i);
//                                    candidate->getArg(i)->dump();
                                    if (func_arg.find(opt) != func_arg.end()) {
                                        sub_arg[candidate->getArg(i)] = func_arg[opt];
//                                        func_arg[opt]->dump();
                                    }
                                    else {
                                        sub_arg[candidate->getArg(i)] = opt;
//                                        opt->dump();
                                    }
                                }
                                calCallPtr(candidate, sub_sub_ret, phi_func, sub_arg);
                                if (sub_sub_ret != nullptr) {
//                                    sub_sub_ret->dump();
                                    sub_ret[call].insert(sub_sub_ret);
                                }
                            }
//                            func_arg[call_ptr]->dump();
                        }
                        // ?????????????????????
                        else if (sub_ret.find(call_ptr) != sub_ret.end()) {
                            unsigned int col = I.getDebugLoc().getLine();
                            for (Value * opt : sub_ret[call_ptr]) {
                                if (auto sub_func = dyn_cast<Function>(opt)) {
                                    mResult[col].insert(sub_func);
                                }
                                else if (auto sub_phi = dyn_cast<PHINode>(opt)) {
                                    if (phi_func.find(sub_phi) != phi_func.end()) {
                                        for (Function * candidate : phi_func[sub_phi]) {
                                            mResult[col].insert(candidate);
                                        }
                                    }
                                }
                            }
//                            sub_ret[call] = sub_ret[call_ptr];
                        }
                    }
                }
                // return??????
                else if (auto *ret = dyn_cast<ReturnInst>(&I)) {
                    unsigned int col = I.getDebugLoc().getLine();
//                    if (col == 20) {
//                        I.dump();
//                    }
                    Value * opt = ret->getReturnValue();

                    // ???????????????
                    if (func_arg.find(opt) != func_arg.end()) {
                        func_ret = func_arg[opt];
                    }
//                    if (sub_ret.find(opt) != sub_ret.end()) {
//                        for (Value * ret_opt : sub_ret[opt]) {
//                            if (auto sub_func = dyn_cast<Function>(ret_opt)) {
//                                mResult[col].insert(sub_func);
//                            }
//                            else if (auto sub_phi = dyn_cast<PHINode>(ret_opt)) {
//                                if (phi_func.find(sub_phi) != phi_func.end()) {
//                                    for (Function * candidate : phi_func[sub_phi]) {
//                                        mResult[col].insert(candidate);
//                                    }
//                                }
//                            }
//                        }
//                    }
                }
                // phi??????
                else if (auto *phi = dyn_cast<PHINode>(&I)) {
//                    phi->dump();
                    for (unsigned int i = 0, phi_count = phi->getNumIncomingValues(); i < phi_count; ++i) {
                        if (Value * opt = phi->getOperand(i)) {
//                            opt->dump();
                            // ???????????????????????????
                            if (auto callee = dyn_cast<Function>(opt)) {
                                phi_func[phi].insert(callee);
                            }
                            // ????????????phi?????????????????????
                            else if (auto sub_phi = dyn_cast<PHINode>(opt)) {
                                for (Function * candidate : phi_func[sub_phi]) {
                                    phi_func[phi].insert(candidate);

                                }
                            }
                            // ????????????????????????
                            else if (func_arg.find(opt) != func_arg.end()) {
                                // ???????????????
                                if (auto candidate = dyn_cast<Function>(func_arg[opt])) {
                                    phi_func[phi].insert(candidate);
                                }
                                // phi?????????????????????
                                else if (auto sub_phi = dyn_cast<PHINode>(func_arg[opt])) {
                                    for (Function * candidate : phi_func[sub_phi]) {
                                        phi_func[phi].insert(candidate);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    void display() {
        for (auto &it : mResult) {
            errs() << it.first << " : ";
            bool comma = false;
            for (Function * func : it.second) {
                if (comma) {
                    errs() << ", ";
                }
                errs() << func->getName();
                comma = true;
            }
            errs() << "\n";
        }
    }

    bool runOnModule(Module &M) override {
//        errs().write_escaped(M.getName()) << "\n";
//        M.dump();
//        errs() << "-------------------------\n";
        // map phi -> functions
        std::map<Value *, std::set<Function *>> phi_func{};
        // save the arg of callee
        std::map<Value *, Value *> func_arg{};
        // save the return value
        Value * ret;
        for (Function &func : M) {
            if (!func.isIntrinsic()) {
//                errs() << "Function : " << func.getName() << "\n";
                calCallPtr(&func, ret, phi_func, func_arg);
            }
        }
        display();
        return false;
    }

};


char FuncPtrPass::ID = 0;
static RegisterPass<FuncPtrPass> X("funcptrpass", "Print function call instruction");

static cl::opt<std::string>
        InputFilename(cl::Positional,
                      cl::desc("<filename>.bc"),
                      cl::init(""));


int main(int argc, char **argv) {
    LLVMContext &Context = getGlobalContext();
    SMDiagnostic Err;
    // Parse the command line to read the Inputfilename
    cl::ParseCommandLineOptions(argc, argv,
                                "FuncPtrPass \n My first LLVM too which does not do much.\n");


    // Load the input module
    std::unique_ptr<Module> M = parseIRFile(InputFilename, Err, Context);
    if (!M) {
        Err.print(argv[0], errs());
        return 1;
    }

    llvm::legacy::PassManager Passes;

    ///Remove functions' optnone attribute in LLVM5.0
    Passes.add(new EnableFunctionOptPass());
    ///Transform it to SSA
    Passes.add(llvm::createPromoteMemoryToRegisterPass());

    /// Your pass to print Function and Call Instructions
    Passes.add(new FuncPtrPass());
    Passes.run(*M.get());
}

