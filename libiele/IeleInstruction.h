#pragma once

#include "SymbolTableListTraits.h"

#include "llvm/ADT/iterator_range.h"
#include <vector>

namespace dev {
namespace iele {

class IeleBlock;
template <typename ItemClass> class SymbolTableListTraits;

class IeleInstruction :
  public llvm::ilist_node_with_parent<IeleInstruction, IeleBlock> {
public:
  // The type for the list of operands.
  //using IeleOperandListType = SymbolTableList<IeleValue>;
  using IeleOperandListType = std::vector<IeleValue *>;
  // The type for the list of lvalues.
  //using IeleLValueListType = SymbolTableList<IeleLocalVariable>;
  using IeleLValueListType = std::vector<IeleLocalVariable *>;

  // The operands iterator.
  using iterator = IeleOperandListType::iterator;
  // The lvalues iterator.
  using lvalue_iterator = IeleLValueListType::iterator;

  // The operands constant iterator.
  using const_iterator = IeleOperandListType::const_iterator;
  // The lvalues constant iterator.
  using const_lvalue_iterator = IeleLValueListType::const_iterator;

  // Enumeration of instruction opcodes
  enum IeleOps {
#define FIRST_IELE_OTHER_INST(N) IeleOthersBegin = N,
#define HANDLE_IELE_OTHER_INST(N, OPC, TXT) OPC = N,
#define LAST_IELE_OTHER_INST(N) IeleOthersEnd = N+1,

#define FIRST_IELE_CALL_INST(N) IeleCallsBegin = N,
#define HANDLE_IELE_CALL_INST(N, OPC, TXT) OPC = N,
#define LAST_IELE_CALL_INST(N) IeleCallsEnd = N+1,

#define FIRST_IELE_INTRINSIC_INST(N) IeleIntrinsicsBegin = N,
#define HANDLE_IELE_INTRINSIC_INST(N, OPC, TXT) OPC = N,
#define LAST_IELE_INTRINSIC_INST(N) IeleIntrinsicsEnd = N+1

#include "IeleInstruction.def"
  };

private:
  const IeleOps InstID;
  IeleOperandListType IeleOperandList;
  IeleLValueListType IeleLValueList;
  IeleBlock *Parent;

  // Used by SymbolTableListTraits.
  void setParent(IeleBlock *parent);

  friend class SymbolTableListTraits<IeleInstruction>;

  IeleInstruction(IeleOps opc, IeleInstruction *InsertBefore = nullptr);
  IeleInstruction(IeleOps opc, IeleBlock *InsertAtEnd);

public:
  IeleInstruction(const IeleInstruction&) = delete;
  void operator=(const IeleInstruction&) = delete;

  ~IeleInstruction();

  IeleOps getOpcode() const { return InstID; }

  inline const IeleBlock *getParent() const { return Parent; }
  inline       IeleBlock *getParent()       { return Parent; }

  // Get the operands/lvalues of the IeleInstruction.
  //
  const IeleOperandListType &getIeleOperandList() const {
    return IeleOperandList;
  }
  IeleOperandListType &getIeleOperandList() {
    return IeleOperandList;
  }

  static IeleOperandListType
  IeleInstruction::*getSublistAccess(IeleValue*) {
    return &IeleInstruction::IeleOperandList;
  }

  const IeleLValueListType &getIeleLValueList() const {
    return IeleLValueList;
  }
  IeleLValueListType &getIeleLValueList() {
    return IeleLValueList;
  }

  static IeleLValueListType
  IeleInstruction::*getSublistAccess(IeleLocalVariable*) {
    return &IeleInstruction::IeleLValueList;
  }

  // Return the symbol table of the parent IeleFunction if any, otherwise
  // nullptr.
  //
  IeleValueSymbolTable *getIeleValueSymbolTable();
  const IeleValueSymbolTable *getIeleValueSymbolTable() const;

  // Operand iterator forwarding functions.
  //
  iterator       begin()       { return IeleOperandList.begin(); }
  const_iterator begin() const { return IeleOperandList.begin(); }
  iterator       end  ()       { return IeleOperandList.end();   }
  const_iterator end  () const { return IeleOperandList.end();   }

  llvm::iterator_range<iterator> operands() {
    return llvm::make_range(begin(), end());
  }
  llvm::iterator_range<const_iterator> operands() const {
    return llvm::make_range(begin(), end());
  }

  size_t  size() const { return IeleOperandList.size();  }
  bool   empty() const { return IeleOperandList.empty(); }

  // LValue iterator forwarding functions.
  //
  lvalue_iterator       lvalue_begin()       { return IeleLValueList.begin(); }
  const_lvalue_iterator lvalue_begin() const { return IeleLValueList.begin(); }
  lvalue_iterator       lvalue_end  ()       { return IeleLValueList.end();   }
  const_lvalue_iterator lvalue_end  () const { return IeleLValueList.end();   }

  llvm::iterator_range<lvalue_iterator> lvalues() {
    return llvm::make_range(lvalue_begin(), lvalue_end());
  }
  llvm::iterator_range<const_lvalue_iterator> lvalues() const {
    return llvm::make_range(lvalue_begin(), lvalue_end());
  }

  size_t  lvalue_size() const { return IeleLValueList.size();  }
  bool   lvalue_empty() const { return IeleLValueList.empty(); }

  // Creators for various instruction types.
  static IeleInstruction *CreateRetVoid(
      IeleInstruction *InsertBefore = nullptr);
  static IeleInstruction *CreateRetVoid(IeleBlock *InsertAtEnd);

  static IeleInstruction *CreateRet(
      llvm::SmallVectorImpl<IeleValue *> &ReturnValues,
      IeleInstruction *InsertBefore = nullptr);
  static IeleInstruction *CreateRet(
      llvm::SmallVectorImpl<IeleValue *> &ReturnValues, IeleBlock *InsertAtEnd);

  static IeleInstruction *CreateRevert(
      IeleValue * StatusValue, IeleInstruction *InsertBefore = nullptr);
  static IeleInstruction *CreateRevert(
      IeleValue *StatusValue, IeleBlock *InsertAtEnd);

  static IeleInstruction *CreateIsZero(
      IeleLocalVariable *Result, IeleValue *ConditionValue,
      IeleInstruction *InsertBefore = nullptr);
  static IeleInstruction *CreateIsZero(
      IeleLocalVariable *Result, IeleValue *ConditionValue,
      IeleBlock *InsertAtEnd);

  static IeleInstruction *CreateSelfdestruct(
      IeleValue *Target, IeleInstruction *InsertBefore = nullptr);
  static IeleInstruction *CreateSelfdestruct(
      IeleValue *Target, IeleBlock *InsertAtEnd);

  static IeleInstruction *CreateUncondBr(
      IeleBlock *Target, IeleInstruction *InsertBefore = nullptr);
  static IeleInstruction *CreateUncondBr(
      IeleBlock *Target, IeleBlock *InsertAtEnd);

  static IeleInstruction *CreateCondBr(
      IeleValue *Condition, IeleBlock *Target,
      IeleInstruction *InsertBefore = nullptr);
  static IeleInstruction *CreateCondBr(
      IeleValue *Condition, IeleBlock *Target, IeleBlock *InsertAtEnd);

  static IeleInstruction *CreateAccountCall(
      IeleLocalVariable *StatusValue,
      llvm::SmallVectorImpl<IeleLocalVariable *> &LValues,
      IeleGlobalValue *Callee, IeleValue *AddressValue,
      IeleValue *TransferValue, IeleValue *GasValue,
      llvm::SmallVectorImpl<IeleValue *> &ArgumnentValues,
      IeleInstruction *InsertBefore = nullptr);
  static IeleInstruction *CreateAccountCall(
      IeleLocalVariable *StatusValue,
      llvm::SmallVectorImpl<IeleLocalVariable *> &LValues,
      IeleGlobalValue *Callee, IeleValue *AddressValue,
      IeleValue *TransferValue, IeleValue *GasValue,
      llvm::SmallVectorImpl<IeleValue *> &ArgumnentValues,
      IeleBlock *InsertAtEnd);

  static IeleInstruction *CreateIntrinsicCall(
      IeleOps IntrinsicOpcode, IeleLocalVariable *Result,
      llvm::SmallVectorImpl<IeleValue *> &ArgumnentValues,
      IeleInstruction *InsertBefore = nullptr);
  static IeleInstruction *CreateIntrinsicCall(
      IeleOps IntrinsicOpcode, IeleLocalVariable *Result,
      llvm::SmallVectorImpl<IeleValue *> &ArgumnentValues,
      IeleBlock *InsertAtEnd);

  static IeleInstruction *CreateInternalCall(
      llvm::SmallVectorImpl<IeleLocalVariable *> &LValues,
      IeleGlobalValue *Callee,
      llvm::SmallVectorImpl<IeleValue *> &ArgumnentValues,
      IeleInstruction *InsertBefore = nullptr);
  static IeleInstruction *CreateInternalCall(
      llvm::SmallVectorImpl<IeleLocalVariable *> &LValues,
      IeleGlobalValue *Callee,
      llvm::SmallVectorImpl<IeleValue *> &ArgumnentValues,
      IeleBlock *InsertAtEnd);

  static IeleInstruction *CreateAssign(
      IeleLocalVariable *Result, IeleValue *RHSValue,
      IeleInstruction *InsertBefore = nullptr);
  static IeleInstruction *CreateAssign(
      IeleLocalVariable *Result, IeleValue *RHSValue,
      IeleBlock *InsertAtEnd);

  static IeleInstruction *CreateSLoad(
      IeleLocalVariable *Result, IeleValue *AddressValue,
      IeleInstruction *InsertBefore = nullptr);
  static IeleInstruction *CreateSLoad(
      IeleLocalVariable *Result, IeleValue *AddressValue,
      IeleBlock *InsertAtEnd);

  static IeleInstruction *CreateSStore(
      IeleValue *DataValue, IeleValue *AddressValue,
      IeleInstruction *InsertBefore = nullptr);
  static IeleInstruction *CreateSStore(
      IeleValue *DataValue, IeleValue *AddressValue, IeleBlock *InsertAtEnd);

  static IeleInstruction *CreateLoad(
      IeleLocalVariable *Result, IeleValue *AddressValue,
      IeleInstruction *InsertBefore = nullptr);
  static IeleInstruction *CreateLoad(
      IeleLocalVariable *Result, IeleValue *AddressValue,
      IeleBlock *InsertAtEnd);

  static IeleInstruction *CreateStore(
      IeleValue *DataValue, IeleValue *AddressValue,
      IeleInstruction *InsertBefore = nullptr);
  static IeleInstruction *CreateStore(
      IeleValue *DataValue, IeleValue *AddressValue, IeleBlock *InsertAtEnd);

  static IeleInstruction *CreateBinOp(
      IeleOps BinOpcode, IeleLocalVariable *Result, IeleValue *LeftOperandValue,
      IeleValue *RightOperandValue, IeleInstruction *InsertBefore = nullptr);
  static IeleInstruction *CreateBinOp(
      IeleOps BinOpcode, IeleLocalVariable *Result, IeleValue *LeftOperandValue,
      IeleValue *RightOperandValue, IeleBlock *InsertAtEnd);

  static IeleInstruction *CreateTernOp(
      IeleOps TernOpcode, IeleLocalVariable *Result, IeleValue *FirstOperandValue,
      IeleValue *SecondOperandValue, IeleValue *ThirdOperandValue,
      IeleInstruction *InsertBefore = nullptr);

  static IeleInstruction *CreateTernOp(
      IeleOps TernOpcode, IeleLocalVariable *Result, IeleValue *FirstOperandValue,
      IeleValue *SecondOperandValue, IeleValue *ThirdOperandValue,
      IeleBlock *InsertAtEnd);

  void print(llvm::raw_ostream &OS, unsigned indent = 0) const;
};

} // end namespace iele
} // end namespace dev

namespace llvm {
// Specialization for isa so that we can check f.e. isa<IeleValue> for an
// IeleInstruction. Mainly used by SymbolTableListTraits.
template <> struct isa_impl<dev::iele::IeleValue, dev::iele::IeleInstruction> {
  static inline bool doit(const dev::iele::IeleInstruction &Val) {
    return false;
  }
};

} // end namespace llvm
