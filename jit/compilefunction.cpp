//
// Created by noorall on 2023/7/4.
//

#include "compilefunction.h"
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>

namespace funcexp
{
void compileFunction(llvm::Module& module, const Func& function)
{
  llvm::IRBuilder<> b(module.getContext());
}

CompiledFunction compileFunction(msc_jit::JIT& jit, const Func& function)
{
  auto compiled_module = jit.compileModule([&](llvm::Module& module) { compileFunction(module, function); });
  CompiledFunction compiledFunction;
  return compiledFunction;
}

}  // namespace funcexp