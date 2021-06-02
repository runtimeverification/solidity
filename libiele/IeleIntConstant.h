#pragma once

#include <libsolutil/Common.h>
#include "IeleConstant.h"

namespace solidity {
namespace iele {

class IeleIntConstant : public IeleConstant {
private:

  bigint Val;
  bool PrintAsHex;
protected:
  IeleIntConstant(IeleContext *Ctx, const bigint &V, bool PrintAsHex);

public:
  IeleIntConstant(const IeleIntConstant&) = delete;
  void operator=(const IeleIntConstant&) = delete;

  ~IeleIntConstant() override;

  static IeleIntConstant *Create(IeleContext *Ctx, const bigint &V, bool PrintAsHex = false);

  static IeleIntConstant *getZero(IeleContext *Ctx);
  static IeleIntConstant *getOne(IeleContext *Ctx);
  static IeleIntConstant *getMinusOne(IeleContext *Ctx);

  // Return the constant as a bigint value reference. This allows clients to
  // obtain a full-precision copy of the value.
  inline const bigint &getValue() const {
    return Val;
  }

  void print(llvm::raw_ostream &OS, unsigned indent = 0) const override;

  // Method for support type inquiry through isa, cast, and dyn_cast.
  static bool classof(const IeleValue *V) {
    return V->getIeleValueID() == IeleValue::IeleIntConstantVal;
  }
};

} // end namespace iele
} // end namespace solidity
