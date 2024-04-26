#include "compileoperator.h"
#include "llvm/IR/Verifier.h"
#include <llvm/Support/raw_ostream.h>  // 确保包含了这个头文件
#include "rowgroup.h"

namespace execplan
{

void compileExternalFunction(llvm::Module& module, llvm::IRBuilder<>& b)
{
  auto* func_intToDatetime_type =
      llvm::FunctionType::get(b.getInt64Ty(), {b.getInt64Ty(), b.getInt1Ty()->getPointerTo()}, false);
  llvm::Function::Create(func_intToDatetime_type, llvm::Function::ExternalLinkage,
                         "dataconvert::DataConvert::intToDatetime", module);
  auto* func_timestampValueToInt_type =
      llvm::FunctionType::get(b.getInt64Ty(), {b.getInt64Ty(), b.getInt64Ty()}, false);
  llvm::Function::Create(func_timestampValueToInt_type, llvm::Function::ExternalLinkage,
                         "dataconvert::DataConvert::timestampValueToInt", module);
}

void compileOperator(llvm::Module& module, const execplan::SRCP& expression, rowgroup::Row& row)
{
  auto columns = expression.get()->simpleColumnList();
  llvm::IRBuilder<> b(module.getContext());
  compileExternalFunction(module, b);
  llvm::Type* return_type;
  auto* data_type = b.getInt8Ty()->getPointerTo();
  auto* isNull_type = b.getInt1Ty()->getPointerTo();
  switch (expression->resultType().colDataType)
  {
    case CalpontSystemCatalog::BIGINT:
    case CalpontSystemCatalog::INT:
    case CalpontSystemCatalog::MEDINT:
    case CalpontSystemCatalog::SMALLINT:
    case CalpontSystemCatalog::TINYINT:
    case CalpontSystemCatalog::UBIGINT:
    case CalpontSystemCatalog::UINT:
    case CalpontSystemCatalog::UMEDINT:
    case CalpontSystemCatalog::USMALLINT:
    case CalpontSystemCatalog::UTINYINT: return_type = b.getInt64Ty(); break;
    case CalpontSystemCatalog::DOUBLE:
    case CalpontSystemCatalog::UDOUBLE: return_type = b.getDoubleTy(); break;
    case CalpontSystemCatalog::FLOAT:
    case CalpontSystemCatalog::UFLOAT: return_type = b.getFloatTy(); break;
    default: throw logic_error("compileOperator: unsupported type");
  }
  auto* func_type = llvm::FunctionType::get(return_type, {data_type, isNull_type}, false);
  auto* func =
      llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, expression->alias(), module);
  func->setDoesNotThrow();
  auto* args = func->args().begin();
  llvm::Value* data_ptr = args++;
  llvm::Value* isNull_ptr = args++;
  auto* entry = llvm::BasicBlock::Create(b.getContext(), "entry", func);
  b.SetInsertPoint(entry);
  auto* ret = expression->compile(b, data_ptr, isNull_ptr, row, expression->resultType().colDataType);

  b.CreateRet(ret);

  llvm::verifyFunction(*func);
}

CompiledOperator compileOperator(msc_jit::JIT& jit, const execplan::SRCP& expression, rowgroup::Row& row)
{
  auto compiled_module =
      jit.compileModule([&](llvm::Module& module) { compileOperator(module, expression, row); });
  CompiledOperator result_compiled_function{.compiled_module = compiled_module};

  switch (expression->resultType().colDataType)
  {
    case CalpontSystemCatalog::BIGINT:
    case CalpontSystemCatalog::INT:
    case CalpontSystemCatalog::MEDINT:
    case CalpontSystemCatalog::SMALLINT:
    case CalpontSystemCatalog::TINYINT:
    case CalpontSystemCatalog::DATE:
      result_compiled_function.compiled_function_int64 = reinterpret_cast<JITCompiledOperator<int64_t>>(
          compiled_module.function_name_to_symbol[expression->alias()]);
      break;
    case CalpontSystemCatalog::UBIGINT:
    case CalpontSystemCatalog::UINT:
    case CalpontSystemCatalog::UMEDINT:
    case CalpontSystemCatalog::USMALLINT:
    case CalpontSystemCatalog::UTINYINT:
      result_compiled_function.compiled_function_uint64 = reinterpret_cast<JITCompiledOperator<uint64_t>>(
          compiled_module.function_name_to_symbol[expression->alias()]);
      break;
    case CalpontSystemCatalog::DOUBLE:
    case CalpontSystemCatalog::UDOUBLE:
      result_compiled_function.compiled_function_double = reinterpret_cast<JITCompiledOperator<double>>(
          compiled_module.function_name_to_symbol[expression->alias()]);
      break;
    case CalpontSystemCatalog::FLOAT:
    case CalpontSystemCatalog::UFLOAT:
      result_compiled_function.compiled_function_float = reinterpret_cast<JITCompiledOperator<float>>(
          compiled_module.function_name_to_symbol[expression->alias()]);
    default: throw logic_error("compileOperator: unsupported type");
  }

  return result_compiled_function;
}

}  // namespace execplan