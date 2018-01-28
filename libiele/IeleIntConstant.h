#pragma once

#include "IeleConstant.h"

#include "llvm/ADT/APInt.h"

namespace dev {
namespace iele {

class IeleIntConstant : public IeleConstant {
private:
  // here we should use a true unbound integer datatype but using llvm's APInt
  // for now.
  llvm::APInt Val;

protected:
  IeleIntConstant(IeleContext *Ctx, const llvm::APInt &V);

public:
  IeleIntConstant(const IeleIntConstant&) = delete;
  void operator=(const IeleIntConstant&) = delete;

  ~IeleIntConstant();

  static IeleIntConstant *Create(IeleContext *Ctx, const llvm::APInt &V);

  static IeleIntConstant *getZero(IeleContext *Ctx);
  static IeleIntConstant *getOne(IeleContext *Ctx);
  static IeleIntConstant *getMinusOne(IeleContext *Ctx);

  // Return the constant as an APInt value reference. This allows clients to
  // obtain a full-precision copy of the value.
  inline const llvm::APInt &getValue() const {
    return Val;
  }

  void print(llvm::raw_ostream &OS, unsigned indent = 0) const override;

  // Method for support type inquiry through isa, cast, and dyn_cast.
  static bool classof(const IeleValue *V) {
    return V->getIeleValueID() == IeleValue::IeleIntConstantVal;
  }
};

} // end namespace iele
} // end namespace dev
