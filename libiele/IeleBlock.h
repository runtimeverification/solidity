#pragma once

#include "IeleInstruction.h"
#include "IeleValue.h"
#include "SymbolTableListTraits.h"

#include "llvm/ADT/iterator_range.h"
#include "llvm/ADT/Twine.h"

namespace solidity {
namespace iele {

class IeleValueSymbolTable;

class IeleBlock :
  public IeleValue,
  public llvm::ilist_node_with_parent<IeleBlock, IeleFunction> {
public:
  // The type for the list of instructions.
  using IeleInstructionListType = SymbolTableList<IeleInstruction>;

  // The instruction iterator.
  using iterator = IeleInstructionListType::iterator;
  // The instruction constant iterator.
  using const_iterator = IeleInstructionListType::const_iterator;
  // The instruction reverse iterator.
  using reverse_iterator = IeleInstructionListType::reverse_iterator;
  // The instruction constant reverse iterator.
  using const_reverse_iterator =
    IeleInstructionListType::const_reverse_iterator;

private:
  IeleInstructionListType IeleInstructionList;
  IeleFunction *Parent;

  // Used by SymbolTableListTraits.
  void setParent(IeleFunction *parent);

  friend class SymbolTableListTraits<IeleBlock>;

  // IeleBlock ctor - If the (optional) function parameter is specified, the
  // block is automatically inserted at either the end of the function (if
  // InsertBefore is null), or before the specified block.
  IeleBlock(IeleContext *Ctx, const llvm::Twine &Name= "",
            IeleFunction *F = nullptr, IeleBlock *InsertBefore = nullptr);

public:
  IeleBlock(const IeleBlock&) = delete;
  void operator=(const IeleBlock&) = delete;

  ~IeleBlock() override;

  static IeleBlock *Create(IeleContext *Ctx, const llvm::Twine &Name = "",
                           IeleFunction *F = nullptr,
                           IeleBlock *InsertBefore = nullptr) {
    return new IeleBlock(Ctx, Name, F, InsertBefore);
  }

  inline const IeleFunction *getParent() const { return Parent; }
  inline       IeleFunction *getParent()       { return Parent; }

  // Get the underlying elements of the IeleBlock.
  //
  const IeleInstructionListType &getIeleInstructionList() const {
    return IeleInstructionList;
  }
  IeleInstructionListType &getIeleInstructionList() {
    return IeleInstructionList;
  }

  static IeleInstructionListType
  IeleBlock::*getSublistAccess(IeleInstruction*) {
    return &IeleBlock::IeleInstructionList;
  }

  // Return the symbol table of the parent IeleFunction if any, otherwise
  // nullptr.
  //
  IeleValueSymbolTable *getIeleValueSymbolTable();
  const IeleValueSymbolTable *getIeleValueSymbolTable() const;

  // IeleInstruction iterator forwarding functions.
  //
  iterator       begin()       { return IeleInstructionList.begin(); }
  const_iterator begin() const { return IeleInstructionList.begin(); }
  iterator       end  ()       { return IeleInstructionList.end();   }
  const_iterator end  () const { return IeleInstructionList.end();   }

  reverse_iterator       rbegin()       { return IeleInstructionList.rbegin(); }
  const_reverse_iterator rbegin() const { return IeleInstructionList.rbegin(); }
  reverse_iterator       rend  ()       { return IeleInstructionList.rend();   }
  const_reverse_iterator rend  () const { return IeleInstructionList.rend();   }

  llvm::iterator_range<iterator> instructions() {
    return make_range(begin(), end());
  }
  llvm::iterator_range<const_iterator> instructions() const {
    return make_range(begin(), end());
  }

  size_t  size() const { return IeleInstructionList.size();  }
  bool   empty() const { return IeleInstructionList.empty(); }

  const IeleInstruction &front() const { return IeleInstructionList.front(); }
        IeleInstruction &front()       { return IeleInstructionList.front(); }
  const IeleInstruction  &back() const { return IeleInstructionList.back();  }
        IeleInstruction  &back()       { return IeleInstructionList.back();  }

  // Inserts an unlinked basic block into Parent.  If InsertBefore is
  // provided, inserts before that basic block, otherwise inserts at the end.
  //
  // Precondition: getParent() is nullptr.
  //
  void insertInto(IeleFunction *NewParent, IeleBlock *InsertBefore = nullptr);

  // Returns true if the last instruction of the block is a ret instruction.
  bool endsWithRet() const;

  void print(llvm::raw_ostream &OS, unsigned indent = 0) const override;

  // Method for support type inquiry through isa, cast, and dyn_cast.
  static bool classof(const IeleValue *V) {
    return V->getIeleValueID() == IeleValue::IeleBlockVal;
  }
};

} // end namespace iele
} // end namespace solidity
