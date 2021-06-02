#pragma once

#include "IeleLocalVariable.h"

namespace solidity {
namespace iele {

class IeleArgument :
  public IeleLocalVariable,
  public llvm::ilist_node_with_parent<IeleArgument, IeleFunction> {
private:
  friend class SymbolTableListTraits<IeleArgument>;

  // IeleAgrument ctor - If the (optional) IeleFunction argument is specified,
  // the argument is automatically inserted into the end of the formal argument
  // list for the given function.
  //
  IeleArgument(IeleContext *Ctx, const llvm::Twine &Name= "",
               IeleFunction *F = nullptr);

public:
  IeleArgument(const IeleArgument&) = delete;
  void operator=(const IeleArgument&) = delete;

  ~IeleArgument() override;

  static IeleArgument *Create(IeleContext *Ctx, const llvm::Twine &Name = "",
                              IeleFunction *F = nullptr) {
    return new IeleArgument(Ctx, Name, F);
  }

  // Method for support type inquiry through isa, cast, and dyn_cast.
  static bool classof(const IeleValue *V) {
    return V->getIeleValueID() == IeleValue::IeleArgumentVal;
  }
};

} // end namespace iele
} // end namespace solidity
