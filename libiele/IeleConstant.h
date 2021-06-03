#pragma once

#include "IeleValue.h"

namespace solidity {
namespace iele {

class IeleConstant : public IeleValue {
protected:
  IeleConstant(IeleContext *Ctx, IeleValueTy ivty) : IeleValue(Ctx, ivty) { }

public:
  IeleConstant(const IeleConstant&) = delete;
  void operator=(const IeleConstant&) = delete;

  virtual ~IeleConstant() override;

  // Method for support type inquiry through isa, cast, and dyn_cast.
  static bool classof(const IeleValue *V) {
    return V->getIeleValueID() == IeleValue::IeleIntConstantVal ||
           V->getIeleValueID() == IeleValue::IeleGlobalVariableVal ||
           V->getIeleValueID() == IeleValue::IeleFunctionVal;
  }
};

} // end namespace iele
} // end namespace solidity
