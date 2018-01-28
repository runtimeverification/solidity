#pragma once

#include "IeleValue.h"

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"

namespace llvm {


} // end llvm namespace

namespace dev {
namespace iele {

struct DenseMapAPIntKeyInfo {
  // TODO: Fix the empty and tombstone values to something uncommon.
  static inline llvm::APInt getEmptyKey() {
    llvm::APInt V(64, INT64_MAX - 1, true);
    return V;
  }

  static inline llvm::APInt getTombstoneKey() {
    llvm::APInt V(64, INT64_MAX, true);
    return V;
  }

  static unsigned getHashValue(const llvm::APInt &Key) {
    return static_cast<unsigned>(hash_value(Key));
  }

  static bool isEqual(const llvm::APInt &LHS, const llvm::APInt &RHS) {
    return LHS.getBitWidth() == RHS.getBitWidth() && LHS == RHS;
  }
};

class IeleContext {
public:
  IeleContext();

  IeleContext(IeleContext &) = delete;
  IeleContext &operator=(const IeleContext &) = delete;

  ~IeleContext();

  // OwnedContracts - The set of contracts instantiated in this context, and
  // which will be automatically deleted if this context is deleted.
  llvm::SmallPtrSet<IeleContract*, 4> OwnedContracts;
  
  using IntMapTy = llvm::DenseMap<llvm::APInt, std::unique_ptr<IeleIntConstant>,
                                  DenseMapAPIntKeyInfo>;
  IntMapTy IeleIntConstants;

  llvm::DenseMap<const IeleValue*, IeleName*> IeleNames;
};

} // end namespace iele
} // end namespace dev
