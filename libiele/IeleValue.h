#pragma once

#include "llvm/Support/Casting.h"

#include "llvm/ADT/ilist_node.h"

namespace llvm {

template<typename ValueTy> class StringMapEntry;
class raw_ostream;
class StringRef;
class Twine;

} // end namespace llvm

namespace solidity {
namespace iele {

class IeleArgument;
class IeleBlock;
class IeleConstant;
class IeleContext;
class IeleContract;
class IeleFunction;
class IeleGlobalValue;
class IeleGlobalVariable;
class IeleInstruction;
class IeleIntConstant;
class IeleLocalVariable;
class IeleValue;
template <typename ItemClass> class SymbolTableListTraits;

using IeleName = llvm::StringMapEntry<IeleValue *>;

class IeleValue {
private:
  IeleContext *Context;
  const unsigned char SubclassID;
  bool HasName;

protected:
  IeleValue(IeleContext *Ctx, unsigned char SCID) :
    Context(Ctx), SubclassID(SCID), HasName(false) { }

public:
  virtual ~IeleValue();

  IeleValue(const IeleValue &) = delete;
  IeleValue &operator=(const IeleValue &) = delete;

  bool hasName() const { return HasName; }
  IeleName *getIeleName() const;
  void setIeleName(IeleName *IN);

private:
  void destroyIeleName();

public:
  // Return a constant reference to the value's name.
  //
  // This guaranteed to return the same reference as long as the value is not
  // modified. If the value has a name, this does a hashtable lookup, so it's
  // not free.
  llvm::StringRef getName() const;

  // Change the name of the value. Choose a new unique name if the provided name
  // is taken. Pass "" as parameter if the value's name should be removed.
  void setName(const llvm::Twine &Name);

  // An enumeration for keeping track of the concrete subclass of IeleValue that
  // is actually instantiated. Values of this enumeration are kept in the
  // IeleValue classes SubclassID field. They are used for concrete type
  // identification.
  enum IeleValueTy {
#define HANDLE_IELE_VALUE(Name) Name##Val,
#include "IeleValue.def"
  };

  // Return an ID for the concrete type of this object.
  //
  // This is used to implement the classof checks. This should not be used
  // for any other purpose, as the values may change as IELE evolves.
  unsigned getIeleValueID() const { return SubclassID; }

  //inline const IeleInstruction *getParent() const { return Parent; }
  //inline       IeleInstruction *getParent()       { return Parent; }

  virtual void print(llvm::raw_ostream &OS, unsigned indent = 0) const =0;
  void printAsValue(llvm::raw_ostream &OS) const;
};

} // end namespace iele
} // end namespace solidity

namespace llvm {

// isa - Provide some specializations of isa so that we don't have to include
// the subtype header files to test to see if the value is a subclass...
//
template <> struct isa_impl<solidity::iele::IeleArgument, solidity::iele::IeleValue> {
  static inline bool doit(const solidity::iele::IeleValue &Val) {
    return Val.getIeleValueID() == solidity::iele::IeleValue::IeleArgumentVal;
  }
};

template <> struct isa_impl<solidity::iele::IeleBlock, solidity::iele::IeleValue> {
  static inline bool doit(const solidity::iele::IeleValue &Val) {
    return Val.getIeleValueID() == solidity::iele::IeleValue::IeleBlockVal;
  }
};

template <> struct isa_impl<solidity::iele::IeleContract, solidity::iele::IeleValue> {
  static inline bool doit(const solidity::iele::IeleValue &Val) {
    return Val.getIeleValueID() == solidity::iele::IeleValue::IeleContractVal;
  }
};

template <>
struct isa_impl<solidity::iele::IeleLocalVariable, solidity::iele::IeleValue> {
  static inline bool doit(const solidity::iele::IeleValue &Val) {
    return isa<solidity::iele::IeleArgument>(Val) ||
           Val.getIeleValueID() == solidity::iele::IeleValue::IeleLocalVariableVal;
  }
};

template <> struct isa_impl<solidity::iele::IeleFunction, solidity::iele::IeleValue> {
  static inline bool doit(const solidity::iele::IeleValue &Val) {
    return Val.getIeleValueID() == solidity::iele::IeleValue::IeleFunctionVal;
  }
};

template <>
struct isa_impl<solidity::iele::IeleGlobalVariable, solidity::iele::IeleValue> {
  static inline bool doit(const solidity::iele::IeleValue &Val) {
    return Val.getIeleValueID() == solidity::iele::IeleValue::IeleGlobalVariableVal;
  }
};

template <> struct isa_impl<solidity::iele::IeleIntConstant, solidity::iele::IeleValue> {
  static inline bool doit(const solidity::iele::IeleValue &Val) {
    return Val.getIeleValueID() == solidity::iele::IeleValue::IeleIntConstantVal;
  }
};

template <> struct isa_impl<solidity::iele::IeleGlobalValue, solidity::iele::IeleValue> {
  static inline bool doit(const solidity::iele::IeleValue &Val) {
    return isa<solidity::iele::IeleGlobalVariable>(Val) ||
           isa<solidity::iele::IeleFunction>(Val);
  }
};

template <> struct isa_impl<solidity::iele::IeleConstant, solidity::iele::IeleValue> {
  static inline bool doit(const solidity::iele::IeleValue &Val) {
    return isa<solidity::iele::IeleGlobalValue>(Val) ||
           isa<solidity::iele::IeleIntConstant>(Val);
  }
};

} // end namespace llvm


