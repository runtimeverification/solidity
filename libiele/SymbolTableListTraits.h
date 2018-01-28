#pragma once

#include "IeleValueSymbolTable.h"

#include "llvm/ADT/ilist.h"
#include <cstddef>

namespace dev {
namespace iele {

class IeleArgument;
class IeleBlock;
class IeleContract;
class IeleFunction;
class IeleGlobalVariable;
class IeleInstruction;
class IeleLocalVariable;
class IeleValueSymbolTable;

// Template metafunction to get the parent type for a symbol table list.
//
// Implementations create a typedef called type so that we only need a
// single template parameter for the list and traits.
template <typename NodeTy> struct SymbolTableListParentType {};

#define DEFINE_SYMBOL_TABLE_PARENT_TYPE(NODE, PARENT)                          \
  template <> struct SymbolTableListParentType<NODE> { using type = PARENT; };
DEFINE_SYMBOL_TABLE_PARENT_TYPE(IeleInstruction, IeleBlock)
DEFINE_SYMBOL_TABLE_PARENT_TYPE(IeleArgument, IeleFunction)
DEFINE_SYMBOL_TABLE_PARENT_TYPE(IeleBlock, IeleFunction)
DEFINE_SYMBOL_TABLE_PARENT_TYPE(IeleLocalVariable, IeleFunction)
DEFINE_SYMBOL_TABLE_PARENT_TYPE(IeleFunction, IeleContract)
DEFINE_SYMBOL_TABLE_PARENT_TYPE(IeleGlobalVariable, IeleContract)
DEFINE_SYMBOL_TABLE_PARENT_TYPE(IeleContract, IeleContract)
#undef DEFINE_SYMBOL_TABLE_PARENT_TYPE

template <typename NodeTy> class SymbolTableList;

// ItemClass   - The type of objects that I hold, e.g. IeleInstruction.
// ItemParentClass - The type of object that owns the list, e.g. IeleBlock.
//
template <typename ItemClass>
class SymbolTableListTraits :
  public llvm::ilist_alloc_traits<ItemClass> {
  using ListTy = SymbolTableList<ItemClass>;
  using iterator = typename llvm::simple_ilist<ItemClass>::iterator;
  using ItemParentClass =
      typename SymbolTableListParentType<ItemClass>::type;

public:
  SymbolTableListTraits() = default;

private:
  /// getListOwner - Return the object that owns this list. If this is a list
  /// of instructions, it returns the BasicBlock that owns them.
  ItemParentClass *getListOwner() {
    size_t Offset(size_t(&((ItemParentClass*)nullptr->*ItemParentClass::
                           getSublistAccess(static_cast<ItemClass*>(nullptr)))));
    ListTy *Anchor(static_cast<ListTy *>(this));
    return reinterpret_cast<ItemParentClass*>(reinterpret_cast<char*>(Anchor)-
                                              Offset);
  }

public:
  static ListTy &getList(ItemParentClass *Par) {
    return Par->*(Par->getSublistAccess((ItemClass*)nullptr));
  }

  static IeleValueSymbolTable *getSymTab(ItemParentClass *Par) {
    return Par ? toPtr(Par->getIeleValueSymbolTable()) : nullptr;
  }

  void addNodeToList(ItemClass *V);
  void removeNodeFromList(ItemClass *V);
  void transferNodesFromList(SymbolTableListTraits &L2, iterator first,
                             iterator last);
  // private:
  template<typename TPtr>
  void setSymTabObjectOneLevel(TPtr *, TPtr);
  template<typename TPtr, typename T>
  void setSymTabObjectTwoLevels(TPtr *, TPtr);
  static IeleValueSymbolTable *toPtr(IeleValueSymbolTable *P) { return P; }
  static IeleValueSymbolTable *toPtr(IeleValueSymbolTable &R) { return &R; }
};

template <typename ItemClass>
void SymbolTableListTraits<ItemClass>::addNodeToList(
    ItemClass *V) {
  assert(!V->getParent() && "Value already in a container!!");
  ItemParentClass *Owner = getListOwner();
  V->setParent(Owner);
  if (IeleValue *IV = llvm::dyn_cast<IeleValue>(V))
    if (IV->hasName())
      if (IeleValueSymbolTable *ST = getSymTab(Owner))
        ST->reinsertIeleValue(IV);
}

template <typename ItemClass>
void SymbolTableListTraits<ItemClass>::removeNodeFromList(
    ItemClass *V) {
  V->setParent(nullptr);
  if (IeleValue *IV = llvm::dyn_cast<IeleValue>(V))
    if (IV->hasName())
      if (IeleValueSymbolTable *ST = getSymTab(getListOwner()))
        ST->removeIeleName(IV->getIeleName());
}

template <typename ItemClass>
void SymbolTableListTraits<ItemClass>::transferNodesFromList(
    SymbolTableListTraits &L2, iterator first, iterator last) {
  // We only have to do work here if transferring instructions between blocks
  // or local variables between instructions.
  ItemParentClass *NewIP = getListOwner(), *OldIP = L2.getListOwner();
  assert(NewIP != OldIP && "Expected different list owners");

  // We only have to update symbol table entries if we are transferring the
  // instructions or local variables to a different symtab object...
  IeleValueSymbolTable *NewST = getSymTab(NewIP);
  IeleValueSymbolTable *OldST = getSymTab(OldIP);
  if (NewST != OldST) {
    for (; first != last; ++first) {
      ItemClass &V = *first;
      IeleValue *IV = llvm::dyn_cast<IeleValue>(&V);
      bool HasName = IV && IV->hasName();
      if (OldST && HasName)
        OldST->removeIeleName(IV->getIeleName());
      V.setParent(NewIP);
      if (NewST && HasName)
        NewST->reinsertIeleValue(IV);
    }
  } else {
    // Just transferring between blocks or instructions in the same function,
    // simply update the parent fields in the instructions or local variables...
    for (; first != last; ++first)
      first->setParent(NewIP);
  }
}

// List that automatically updates parent links and symbol tables.
//
// When nodes are inserted into and removed from this list, the associated
// symbol table will be automatically updated.  Similarly, parent links get
// updated automatically.
template <class T>
class SymbolTableList :
  public llvm::iplist_impl<llvm::simple_ilist<T>, SymbolTableListTraits<T>> {};

} // end namespace iele
} // end namespace dev
