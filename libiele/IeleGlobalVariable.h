#pragma once

#include "IeleGlobalValue.h"

#include "llvm/ADT/Twine.h"

namespace solidity {
namespace iele {

class IeleGlobalVariable :
  public IeleGlobalValue,
  public llvm::ilist_node_with_parent<IeleGlobalVariable, IeleFunction> {
private:
  IeleIntConstant *StorageAddress;

  friend class SymbolTableListTraits<IeleGlobalVariable>;

  // IeleGlobalVariable ctor - If the (optional) IeleContract argument is
  // specified, the variable is automatically inserted into the end of the
  // globals list for the given contract.
  //
  IeleGlobalVariable(IeleContext *Ctx, const llvm::Twine &Name= "",
                     IeleContract *C = nullptr);

public:
  IeleGlobalVariable(const IeleGlobalVariable&) = delete;
  void operator=(const IeleGlobalVariable&) = delete;

  ~IeleGlobalVariable() override;

  static IeleGlobalVariable *Create(IeleContext *Ctx,
                                    const llvm::Twine &Name = "",
                                    IeleContract *C = nullptr) {
    return new IeleGlobalVariable(Ctx, Name, C);
  }

  inline IeleIntConstant *getStorageAddress() const {
    return StorageAddress;
  }

  inline void setStorageAddress(IeleIntConstant *Address) {
    StorageAddress = Address;
  }

  void print(llvm::raw_ostream &OS, unsigned indent = 0) const override;

  // Method for support type inquiry through isa, cast, and dyn_cast.
  static bool classof(const IeleValue *V) {
    return V->getIeleValueID() == IeleValue::IeleGlobalVariableVal;
  }
};

} // end namespace iele
} // end namespace solidity
