#pragma once

#include "IeleValue.h"

#include "llvm/ADT/Twine.h"

namespace solidity {
namespace iele {

class IeleLocalVariable :
  public IeleValue,
  public llvm::ilist_node_with_parent<IeleLocalVariable, IeleFunction> {
protected:
  IeleFunction *Parent;

  // Used by SymbolTableListTraits.
  void setParent(IeleFunction *parent) { Parent = parent; }

  friend class SymbolTableListTraits<IeleLocalVariable>;

  // IeleLocalVariable ctor - If the (optional) IeleFunction argument is
  // specified, the argument is automatically inserted into the end of the
  // local variable or formal argument list for the given function.
  //
  IeleLocalVariable(IeleContext *Ctx, const llvm::Twine &Name = "",
                    IeleFunction *F = nullptr, bool isArgument = false);

public:
  IeleLocalVariable(const IeleLocalVariable&) = delete;
  void operator=(const IeleLocalVariable&) = delete;

  virtual ~IeleLocalVariable() override;

  static IeleLocalVariable *Create(IeleContext *Ctx,
                                   const llvm::Twine &Name = "",
                                   IeleFunction *F = nullptr) {
    return new IeleLocalVariable(Ctx, Name, F);
  }

  inline const IeleFunction *getParent() const { return Parent; }
  inline       IeleFunction *getParent()       { return Parent; }

  void print(llvm::raw_ostream &OS, unsigned indent = 0) const override;

  // Method for support type inquiry through isa, cast, and dyn_cast.
  static bool classof(const IeleValue *V) {
    return V->getIeleValueID() == IeleValue::IeleArgumentVal ||
           V->getIeleValueID() == IeleValue::IeleLocalVariableVal;
  }
};

} // end namespace iele
} // end namespace solidity
