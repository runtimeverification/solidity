#include "IeleGlobalValue.h"

using namespace dev;
using namespace dev::iele;

IeleGlobalValue::IeleGlobalValue(IeleContext *Ctx, const llvm::Twine &Name,
                                 IeleValueTy ivty) :
  IeleConstant(Ctx, ivty), Parent(nullptr) { setName(Name); }

IeleGlobalValue::~IeleGlobalValue() { }
