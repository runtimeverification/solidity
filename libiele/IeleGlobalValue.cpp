#include "IeleGlobalValue.h"

using namespace solidity;
using namespace solidity::iele;

IeleGlobalValue::IeleGlobalValue(IeleContext *Ctx, const llvm::Twine &Name,
                                 IeleValueTy ivty) :
  IeleConstant(Ctx, ivty), Parent(nullptr) { setName(Name); }

IeleGlobalValue::~IeleGlobalValue() { }
