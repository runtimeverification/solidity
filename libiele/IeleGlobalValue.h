#pragma once

#include "IeleConstant.h"

namespace solidity {
namespace iele {

class IeleGlobalValue : public IeleConstant {
protected:
  IeleContract *Parent;

  // Used by SymbolTableListTraits.
  void setParent(IeleContract *parent) { Parent = parent; }

  IeleGlobalValue(IeleContext *Ctx, const llvm::Twine &Name, IeleValueTy ivty);

public:
  IeleGlobalValue(const IeleGlobalValue&) = delete;
  void operator=(const IeleGlobalValue&) = delete;

  virtual ~IeleGlobalValue() override;

  inline const IeleContract *getParent() const { return Parent; }
  inline       IeleContract *getParent()       { return Parent; }

  // Method for support type inquiry through isa, cast, and dyn_cast.
  static bool classof(const IeleValue *V) {
    return V->getIeleValueID() == IeleValue::IeleGlobalVariableVal ||
           V->getIeleValueID() == IeleValue::IeleFunctionVal;
  }
};

} // end namespace iele
} // end namespace solidity
