#pragma once

#include "IeleValue.h"

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"

#include <libdevcore/Common.h>
#include <map>

namespace llvm {


} // end llvm namespace

namespace dev {
namespace iele {

class IeleContext {
public:
  IeleContext();

  IeleContext(IeleContext &) = delete;
  IeleContext &operator=(const IeleContext &) = delete;

  ~IeleContext();

  // OwnedContracts - The set of contracts instantiated in this context, and
  // which will be automatically deleted if this context is deleted.
  llvm::SmallPtrSet<IeleContract*, 4> OwnedContracts;
  
  using IntMapTy = std::map<bigint, std::unique_ptr<IeleIntConstant>>;
  IntMapTy IeleIntConstants;

  llvm::DenseMap<const IeleValue*, IeleName*> IeleNames;
};

} // end namespace iele
} // end namespace dev
