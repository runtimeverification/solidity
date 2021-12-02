#include "IeleInstruction.h"

#include "IeleContract.h"
#include "IeleFunction.h"
#include "IeleGlobalVariable.h"

#include <liblangutil/Exceptions.h>

#include "llvm/Support/raw_ostream.h"

using namespace solidity;
using namespace solidity::iele;
using namespace solidity::langutil;

IeleInstruction::IeleInstruction(IeleOps opc, const SourceLocation &Loc,
                                 IeleInstruction *InsertBefore) :
  InstID(opc), Parent(nullptr), Location(Loc) {
  // If requested, insert this instruction into a IeleBlock.
  if (InsertBefore) {
    IeleBlock *B = InsertBefore->getParent();
    solAssert(B, "Instruction to insert before is not in a block!");
    B->getIeleInstructionList().insert(InsertBefore->getIterator(), this);
  }
}

IeleInstruction::IeleInstruction(IeleOps opc, const SourceLocation &Loc,
                                 IeleBlock *InsertAtEnd) :
  InstID(opc), Parent(nullptr), Location(Loc) {
  // Append this instruction into the given IeleBlock.
  solAssert(InsertAtEnd, "Block to append to may not be NULL!");
  InsertAtEnd->getIeleInstructionList().push_back(this);
}

void IeleInstruction::setParent(IeleBlock *parent) {
  // When the given parent block is not null, we need to assert that
  // all operands and lvalues have the same parent as the given parent block.
  if (parent) {
    IeleFunction *F = parent->getParent();
    for (const IeleValue *V : operands()) {
      if (const IeleLocalVariable *LV = llvm::dyn_cast<IeleLocalVariable>(V))
        solAssert(LV->getParent() == F,
               "Instruction operand belongs to a different function!");
      else if (const IeleBlock *B = llvm::dyn_cast<IeleBlock>(V))
        solAssert(B->getParent() == F,
               "Instruction operand belongs to a different function!");
    }

    for (const IeleLocalVariable *LV : lvalues()) {
      solAssert(LV->getParent() == F, 
             "Instruction lvalue belongs to a different function!");
    }
  }

  // Set the parent.
  Parent = parent;
}

IeleValueSymbolTable *IeleInstruction::getIeleValueSymbolTable() {
  if (IeleBlock *B = getParent())
    if (IeleFunction *F = B->getParent())
      return F->getIeleValueSymbolTable();
  return nullptr;
}

const IeleValueSymbolTable *IeleInstruction::getIeleValueSymbolTable() const {
  if (const IeleBlock *B = getParent())
    if (const IeleFunction *F = B->getParent())
      return F->getIeleValueSymbolTable();
  return nullptr;
}

IeleInstruction::~IeleInstruction() { }

void IeleInstruction::removeFromParent() {
  getParent()->getIeleInstructionList().remove(getIterator());
}

SymbolTableList<IeleInstruction>::iterator IeleInstruction::eraseFromParent() {
  return getParent()->getIeleInstructionList().erase(getIterator());
}

IeleInstruction *IeleInstruction::CreateRetVoid(const SourceLocation &Loc, IeleInstruction *InsertBefore) {
  return new IeleInstruction(Ret, Loc, InsertBefore);
}

IeleInstruction *IeleInstruction::CreateRetVoid(const SourceLocation &Loc, IeleBlock *InsertAtEnd) {
  return new IeleInstruction(Ret, Loc, InsertAtEnd);
}

IeleInstruction *IeleInstruction::CreateLog(
    llvm::SmallVectorImpl<IeleValue *> &IndexedArguments,
    IeleValue * NonIndexedArguments,
    const SourceLocation &Loc, IeleInstruction *InsertBefore) {

  IeleInstruction *LogInst = new IeleInstruction(Log, Loc, InsertBefore);
  
  LogInst->getIeleOperandList().insert(LogInst->end(), NonIndexedArguments);
  LogInst->getIeleOperandList().insert(LogInst->end(), 
                                       IndexedArguments.begin(), 
                                       IndexedArguments.end());
  return LogInst;
}

IeleInstruction *IeleInstruction::CreateLog(
    llvm::SmallVectorImpl<IeleValue *> &IndexedArguments,
    IeleValue * NonIndexedArguments,
    const SourceLocation &Loc, IeleBlock *InsertAtEnd) {

  IeleInstruction *LogInst = new IeleInstruction(Log, Loc, InsertAtEnd);
  
  LogInst->getIeleOperandList().insert(LogInst->end(), NonIndexedArguments);
  LogInst->getIeleOperandList().insert(LogInst->end(), 
                                       IndexedArguments.begin(), 
                                       IndexedArguments.end());
  return LogInst;
}

IeleInstruction *IeleInstruction::CreateRet(
    llvm::SmallVectorImpl<IeleValue *> &ReturnValues,
    const SourceLocation &Loc, IeleInstruction *InsertBefore) {
  if (ReturnValues.size() == 0) {
    return CreateRetVoid(Loc, InsertBefore);
  }

  IeleInstruction *RetInst = new IeleInstruction(Ret, Loc, InsertBefore);
  RetInst->getIeleOperandList().insert(RetInst->end(), ReturnValues.begin(), 
                                       ReturnValues.end());

  return RetInst;
}

IeleInstruction *IeleInstruction::CreateRet(
    llvm::SmallVectorImpl<IeleValue *> &ReturnValues, const SourceLocation &Loc, IeleBlock *InsertAtEnd) {
  if (ReturnValues.size() == 0) {
    return CreateRetVoid(Loc, InsertAtEnd);
  }

  IeleInstruction *RetInst = new IeleInstruction(Ret, Loc, InsertAtEnd);
  RetInst->getIeleOperandList().insert(RetInst->end(), ReturnValues.begin(),
                                       ReturnValues.end());

  return RetInst;
}

IeleInstruction *IeleInstruction::CreateRevert(
    IeleValue * StatusValue, const SourceLocation &Loc, IeleInstruction *InsertBefore) {
  solAssert(StatusValue, "CreateRevert: Invalid operands");

  IeleInstruction *RevertInst = new IeleInstruction(Revert, Loc, InsertBefore);
  RevertInst->getIeleOperandList().push_back(StatusValue);

  return RevertInst;
}

IeleInstruction *IeleInstruction::CreateRevert(
    IeleValue *StatusValue, const SourceLocation &Loc, IeleBlock *InsertAtEnd) {
  solAssert(StatusValue, "CreateRevert: Invalid operands");

  IeleInstruction *RevertInst = new IeleInstruction(Revert, Loc, InsertAtEnd);
  RevertInst->getIeleOperandList().push_back(StatusValue);

  return RevertInst;
}

IeleInstruction *IeleInstruction::CreateIsZero(
    IeleLocalVariable *Result, IeleValue *ConditionValue,
    const SourceLocation &Loc, IeleInstruction *InsertBefore) {
  solAssert(Result, "CreateIsZero: Invalid lvalues");
  solAssert(ConditionValue, "CreateIsZero: Invalid operands");

  IeleInstruction *IsZeroInst = new IeleInstruction(IsZero, Loc, InsertBefore);
  IsZeroInst->getIeleLValueList().push_back(Result);
  IsZeroInst->getIeleOperandList().push_back(ConditionValue);

  return IsZeroInst;
}

IeleInstruction *IeleInstruction::CreateIsZero(
    IeleLocalVariable *Result, IeleValue *ConditionValue,
    const SourceLocation &Loc, IeleBlock *InsertAtEnd) {
  solAssert(Result, "CreateIsZero: Invalid lvalues");
  solAssert(ConditionValue, "CreateIsZero: Invalid operands");

  IeleInstruction *IsZeroInst = new IeleInstruction(IsZero, Loc, InsertAtEnd);
  IsZeroInst->getIeleLValueList().push_back(Result);
  IsZeroInst->getIeleOperandList().push_back(ConditionValue);

  return IsZeroInst;
}

// NB: this is not currently used, but it has been added here for
// consistency. Remove...?
IeleInstruction *IeleInstruction::CreateSelfdestruct(
    IeleValue *Target, const SourceLocation &Loc, IeleInstruction *InsertBefore) {
  solAssert(Target, "Selfdestruct: Invalid target");

  IeleInstruction *SelfdestructInst = new IeleInstruction(Selfdestruct, Loc, InsertBefore);
  SelfdestructInst->getIeleOperandList().push_back(Target);

  return SelfdestructInst;
}

IeleInstruction *IeleInstruction::CreateSelfdestruct(
    IeleValue *Target, const SourceLocation &Loc, IeleBlock *InsertAtEnd) {
  solAssert(Target, "Selfdestruct: Invalid target");

  IeleInstruction *SelfdestructInst = new IeleInstruction(Selfdestruct, Loc, InsertAtEnd);
  SelfdestructInst->getIeleOperandList().push_back(Target);

  return SelfdestructInst;
}

IeleInstruction *IeleInstruction::CreateUncondBr(
    IeleBlock *Target, const SourceLocation &Loc, IeleInstruction *InsertBefore) {
  solAssert(Target, "CreateUncondBr: Invalid operands");

  IeleInstruction *UncondBrInst = new IeleInstruction(Br, Loc, InsertBefore);
  UncondBrInst->getIeleOperandList().push_back(Target);

  return UncondBrInst;
}

IeleInstruction *IeleInstruction::CreateUncondBr(
    IeleBlock *Target, const SourceLocation &Loc, IeleBlock *InsertAtEnd) {
  solAssert(Target, "CreateUncondBr: Invalid operands");

  IeleInstruction *UncondBrInst = new IeleInstruction(Br, Loc, InsertAtEnd);
  UncondBrInst->getIeleOperandList().push_back(Target);

  return UncondBrInst;
}

IeleInstruction *IeleInstruction::CreateCondBr(
    IeleValue *Condition, IeleBlock *Target, const SourceLocation &Loc, IeleInstruction *InsertBefore) {
  solAssert(Condition && Target, "CreateCondBr: Invalid operands");

  IeleInstruction *CondBrInst = new IeleInstruction(Br, Loc, InsertBefore);
  CondBrInst->getIeleOperandList().push_back(Condition);
  CondBrInst->getIeleOperandList().push_back(Target);

  return CondBrInst;
}

IeleInstruction *IeleInstruction::CreateCondBr(
    IeleValue *Condition, IeleBlock *Target, const SourceLocation &Loc, IeleBlock *InsertAtEnd) {
  solAssert(Condition && Target, "CreateCondBr: Invalid operands");

  IeleInstruction *CondBrInst = new IeleInstruction(Br, Loc, InsertAtEnd);
  CondBrInst->getIeleOperandList().push_back(Condition);
  CondBrInst->getIeleOperandList().push_back(Target);

  return CondBrInst;
}

IeleInstruction *IeleInstruction::CreateCreate(
    bool Copy,
    IeleLocalVariable *StatusValue,
    IeleLocalVariable *ReturnValue,
    IeleContract *Contract,
    IeleValue *AddressValue,
    IeleValue *TransferValue,
    llvm::SmallVectorImpl<IeleValue *> &ArgumentValues,
    const SourceLocation &Loc, IeleInstruction *InsertBefore) {
  solAssert(StatusValue && ReturnValue && TransferValue,
            "CreateCreate: Invalid operands");
  solAssert(!Contract == Copy,
            "CreateCreate: Invalid operands");
  solAssert(!!AddressValue == Copy,
            "CreateCreate: Invalid Operands");
  IeleInstruction *CreateInst = new IeleInstruction(Copy ? CopyCreate : Create, Loc, InsertBefore);
  CreateInst->getIeleLValueList().push_back(StatusValue);
  CreateInst->getIeleLValueList().push_back(ReturnValue);
  if (Copy) {
    CreateInst->getIeleOperandList().push_back(AddressValue); 
  } else {
    CreateInst->getIeleOperandList().push_back(Contract); 
  }

  CreateInst->getIeleOperandList().push_back(TransferValue);
  CreateInst->getIeleOperandList().insert(CreateInst->end(),
                                        ArgumentValues.begin(),
                                        ArgumentValues.end());

  return CreateInst;  
}

IeleInstruction *IeleInstruction::CreateCreate(
    bool Copy,
    IeleLocalVariable *StatusValue,
    IeleLocalVariable *ReturnValue,
    IeleContract *Contract,
    IeleValue *AddressValue,
    IeleValue *TransferValue,
    llvm::SmallVectorImpl<IeleValue *> &ArgumentValues,
    const SourceLocation &Loc, IeleBlock *InsertAtEnd) {
  solAssert(StatusValue && ReturnValue && TransferValue,
            "CreateCreate: Invalid operands");
  solAssert(!Contract == Copy,
            "CreateCreate: Invalid operands");
  solAssert(!!AddressValue == Copy,
            "CreateCreate: Invalid Operands");
  IeleInstruction *CreateInst = new IeleInstruction(Copy ? CopyCreate : Create, Loc, InsertAtEnd);
  CreateInst->getIeleLValueList().push_back(StatusValue);
  CreateInst->getIeleLValueList().push_back(ReturnValue);
  if (Copy) {
    CreateInst->getIeleOperandList().push_back(AddressValue); 
  } else {
    CreateInst->getIeleOperandList().push_back(Contract); 
  }

  CreateInst->getIeleOperandList().push_back(TransferValue);
  CreateInst->getIeleOperandList().insert(CreateInst->end(),
                                        ArgumentValues.begin(),
                                        ArgumentValues.end());

  return CreateInst;  
}

IeleInstruction *IeleInstruction::CreateAccountCall(
    bool StaticCall,
    IeleLocalVariable *StatusValue,
    llvm::SmallVectorImpl<IeleLocalVariable *> &LValues,
    IeleValue *Callee, IeleValue *AddressValue,
    IeleValue *TransferValue, IeleValue *GasValue,
    llvm::SmallVectorImpl<IeleValue *> &ArgumentValues,
    const SourceLocation &Loc, IeleInstruction *InsertBefore) {
  solAssert(Callee && StatusValue && AddressValue && GasValue,
            "CreateAccountCall: Invalid operands");
  solAssert(!TransferValue == StaticCall,
            "CreateAccountCall: Invalid operands");
  IeleInstruction *AccountCallInst = new IeleInstruction(StaticCall ? StaticCallAt : CallAt, Loc, InsertBefore);
  AccountCallInst->getIeleLValueList().push_back(StatusValue);
  AccountCallInst->getIeleLValueList().insert(AccountCallInst->lvalue_end(),
                                              LValues.begin(), LValues.end());
  AccountCallInst->getIeleOperandList().push_back(Callee);
  AccountCallInst->getIeleOperandList().push_back(AddressValue);
  if (TransferValue) {
    AccountCallInst->getIeleOperandList().push_back(TransferValue);
  }
  AccountCallInst->getIeleOperandList().push_back(GasValue);
  AccountCallInst->getIeleOperandList().insert(AccountCallInst->end(),
                                        ArgumentValues.begin(),
                                        ArgumentValues.end());

  return AccountCallInst;
}

IeleInstruction *IeleInstruction::CreateAccountCall(
    bool StaticCall,
    IeleLocalVariable *StatusValue,
    llvm::SmallVectorImpl<IeleLocalVariable *> &LValues,
    IeleValue *Callee, IeleValue *AddressValue,
    IeleValue *TransferValue, IeleValue *GasValue,
    llvm::SmallVectorImpl<IeleValue *> &ArgumentValues,
    const SourceLocation &Loc, IeleBlock *InsertAtEnd) {
  solAssert(Callee && StatusValue && AddressValue && GasValue,
            "CreateAccountCall: Invalid operands");
  solAssert(!TransferValue == StaticCall,
            "CreateAccountCall: Invalid operands");

  IeleInstruction *AccountCallInst = new IeleInstruction(StaticCall ? StaticCallAt : CallAt, Loc, InsertAtEnd);
  AccountCallInst->getIeleLValueList().push_back(StatusValue);
  AccountCallInst->getIeleLValueList().insert(AccountCallInst->lvalue_end(),
                                              LValues.begin(), LValues.end());
  AccountCallInst->getIeleOperandList().push_back(Callee);
  AccountCallInst->getIeleOperandList().push_back(AddressValue);
  if (TransferValue) {
    AccountCallInst->getIeleOperandList().push_back(TransferValue);
  }
  AccountCallInst->getIeleOperandList().push_back(GasValue);
  AccountCallInst->getIeleOperandList().insert(AccountCallInst->end(),
                                               ArgumentValues.begin(),
                                               ArgumentValues.end());

  return AccountCallInst;
}

IeleInstruction *IeleInstruction::CreateCallAddress(
    IeleLocalVariable *ReturnValue,
    IeleGlobalValue *Callee, IeleValue *AddressValue,
    const SourceLocation &Loc, IeleInstruction *InsertBefore) {
  solAssert(ReturnValue && Callee && AddressValue,
            "CreateCallAddress: Invalid operands");
  IeleInstruction *CallAddressInst = new IeleInstruction(CallAddress, Loc, InsertBefore);
  CallAddressInst->getIeleLValueList().push_back(ReturnValue);
  CallAddressInst->getIeleOperandList().push_back(Callee);
  CallAddressInst->getIeleOperandList().push_back(AddressValue);
  return CallAddressInst;
}

IeleInstruction *IeleInstruction::CreateCallAddress(
    IeleLocalVariable *ReturnValue,
    IeleGlobalValue *Callee, IeleValue *AddressValue,
    const SourceLocation &Loc, IeleBlock *InsertAtEnd) {
  solAssert(ReturnValue && Callee && AddressValue,
            "CreateCallAddress: Invalid operands");
  IeleInstruction *CallAddressInst = new IeleInstruction(CallAddress, Loc, InsertAtEnd);
  CallAddressInst->getIeleLValueList().push_back(ReturnValue);
  CallAddressInst->getIeleOperandList().push_back(Callee);
  CallAddressInst->getIeleOperandList().push_back(AddressValue);
  return CallAddressInst;
}

IeleInstruction *IeleInstruction::CreateIntrinsicCall(
    IeleOps IntrinsicOpcode, IeleLocalVariable *Result,
    llvm::SmallVectorImpl<IeleValue *> &ArgumentValues,
    const SourceLocation &Loc, IeleInstruction *InsertBefore) {
  solAssert(IntrinsicOpcode >= IeleIntrinsicsBegin &&
            IntrinsicOpcode < IeleIntrinsicsEnd,
            "CreateIntrinsicCall: Invalid opcode");
  IeleInstruction *IntrinsicCallInst =
    new IeleInstruction(IntrinsicOpcode, Loc, InsertBefore);
  if (Result)
    IntrinsicCallInst->getIeleLValueList().push_back(Result);
  IntrinsicCallInst->getIeleOperandList().insert(IntrinsicCallInst->end(),
                                                 ArgumentValues.begin(),
                                                 ArgumentValues.end());

  return IntrinsicCallInst;
}

IeleInstruction *IeleInstruction::CreateIntrinsicCall(
    IeleOps IntrinsicOpcode, IeleLocalVariable *Result,
    llvm::SmallVectorImpl<IeleValue *> &ArgumentValues,
    const SourceLocation &Loc, IeleBlock *InsertAtEnd) {
  IeleInstruction *IntrinsicCallInst =
    new IeleInstruction(IntrinsicOpcode, Loc, InsertAtEnd);
  if (Result)
    IntrinsicCallInst->getIeleLValueList().push_back(Result);
  IntrinsicCallInst->getIeleOperandList().insert(IntrinsicCallInst->end(),
                                                 ArgumentValues.begin(),
                                                 ArgumentValues.end());

  return IntrinsicCallInst;
}

IeleInstruction *IeleInstruction::CreateInternalCall(
    llvm::SmallVectorImpl<IeleLocalVariable *> &LValues,
    IeleValue *Callee,
    llvm::SmallVectorImpl<IeleValue *> &ArgumentValues,
    const SourceLocation &Loc, IeleInstruction *InsertBefore) {
  solAssert(Callee, "CreateInternalCall: Invalid operands");

  IeleInstruction *InternalCallInst = new IeleInstruction(Call, Loc, InsertBefore);
  InternalCallInst->getIeleLValueList().insert(InternalCallInst->lvalue_end(),
                                               LValues.begin(), LValues.end());
  InternalCallInst->getIeleOperandList().push_back(Callee);
  InternalCallInst->getIeleOperandList().insert(InternalCallInst->end(),
                                                ArgumentValues.begin(),
                                                ArgumentValues.end());

  return InternalCallInst;
}

IeleInstruction *IeleInstruction::CreateInternalCall(
    llvm::SmallVectorImpl<IeleLocalVariable *> &LValues,
    IeleValue *Callee,
    llvm::SmallVectorImpl<IeleValue *> &ArgumentValues,
    const SourceLocation &Loc, IeleBlock *InsertAtEnd) {
  solAssert(Callee, "CreateInternalCall: Invalid operands");

  IeleInstruction *InternalCallInst = new IeleInstruction(Call, Loc, InsertAtEnd);
  InternalCallInst->getIeleLValueList().insert(InternalCallInst->lvalue_end(),
                                               LValues.begin(), LValues.end());
  InternalCallInst->getIeleOperandList().push_back(Callee);
  InternalCallInst->getIeleOperandList().insert(InternalCallInst->end(),
                                                ArgumentValues.begin(),
                                                ArgumentValues.end());

  return InternalCallInst;
}

IeleInstruction *IeleInstruction::CreateAssign(
    IeleLocalVariable *Result, IeleValue *RHSValue,
    const SourceLocation &Loc, IeleInstruction *InsertBefore ) {
  solAssert(Result, "CreateAssign: Invalid lvalues");
  solAssert(RHSValue, "CreateAssign: Invalid operands");

  IeleInstruction *AssignInst = new IeleInstruction(Assign, Loc, InsertBefore);
  AssignInst->getIeleLValueList().push_back(Result);
  AssignInst->getIeleOperandList().push_back(RHSValue);

  return AssignInst;
}

IeleInstruction *IeleInstruction::CreateAssign(
    IeleLocalVariable *Result, IeleValue *RHSValue,
    const SourceLocation &Loc, IeleBlock *InsertAtEnd) {
  solAssert(Result, "CreateAssign: Invalid lvalues");
  solAssert(RHSValue, "CreateAssign: Invalid operands");

  IeleInstruction *AssignInst = new IeleInstruction(Assign, Loc, InsertAtEnd);
  AssignInst->getIeleLValueList().push_back(Result);
  AssignInst->getIeleOperandList().push_back(RHSValue);

  return AssignInst;
}

IeleInstruction *IeleInstruction::CreateSLoad(
    IeleLocalVariable *Result, IeleValue *AddressValue,
    const SourceLocation &Loc, IeleInstruction *InsertBefore) {
  solAssert(Result, "CreateSLoad: Invalid lvalues");
  solAssert(AddressValue, "CreateSLoad: Invalid operands");

  IeleInstruction *SLoadInst = new IeleInstruction(SLoad, Loc, InsertBefore);
  SLoadInst->getIeleLValueList().push_back(Result);
  SLoadInst->getIeleOperandList().push_back(AddressValue);

  return SLoadInst;
}

IeleInstruction *IeleInstruction::CreateSLoad(
    IeleLocalVariable *Result, IeleValue *AddressValue,
    const SourceLocation &Loc, IeleBlock *InsertAtEnd) {
  solAssert(Result, "CreateSLoad: Invalid lvalues");
  solAssert(AddressValue, "CreateSLoad: Invalid operands");

  IeleInstruction *SLoadInst = new IeleInstruction(SLoad, Loc, InsertAtEnd);
  SLoadInst->getIeleLValueList().push_back(Result);
  SLoadInst->getIeleOperandList().push_back(AddressValue);

  return SLoadInst;
}

IeleInstruction *IeleInstruction::CreateSStore(
    IeleValue *DataValue, IeleValue *AddressValue,
    const SourceLocation &Loc, IeleInstruction *InsertBefore) {
  solAssert(DataValue && AddressValue, "CreateSStore: Invalid operands");

  IeleInstruction *SStoreInst = new IeleInstruction(SStore, Loc, InsertBefore);
  SStoreInst->getIeleOperandList().push_back(DataValue);
  SStoreInst->getIeleOperandList().push_back(AddressValue);

  return SStoreInst;
}

IeleInstruction *IeleInstruction::CreateSStore(
    IeleValue *DataValue, IeleValue *AddressValue, const SourceLocation &Loc, IeleBlock *InsertAtEnd) {
  solAssert(DataValue && AddressValue, "CreateSStore: Invalid operands");

  IeleInstruction *SStoreInst = new IeleInstruction(SStore, Loc, InsertAtEnd);
  SStoreInst->getIeleOperandList().push_back(DataValue);
  SStoreInst->getIeleOperandList().push_back(AddressValue);

  return SStoreInst;
}

IeleInstruction *IeleInstruction::CreateLoad(
    IeleLocalVariable *Result, IeleValue *AddressValue,
    const SourceLocation &Loc, IeleInstruction *InsertBefore) {
  solAssert(Result, "CreateLoad: Invalid lvalues");
  solAssert(AddressValue, "CreateLoad: Invalid operands");

  IeleInstruction *LoadInst = new IeleInstruction(Load, Loc, InsertBefore);
  LoadInst->getIeleLValueList().push_back(Result);
  LoadInst->getIeleOperandList().push_back(AddressValue);

  return LoadInst;
}

IeleInstruction *IeleInstruction::CreateLoad(
    IeleLocalVariable *Result, IeleValue *AddressValue,
    const SourceLocation &Loc, IeleBlock *InsertAtEnd) {
  solAssert(Result, "CreateLoad: Invalid lvalues");
  solAssert(AddressValue, "CreateLoad: Invalid operands");

  IeleInstruction *LoadInst = new IeleInstruction(Load, Loc, InsertAtEnd);
  LoadInst->getIeleLValueList().push_back(Result);
  LoadInst->getIeleOperandList().push_back(AddressValue);

  return LoadInst;
}

IeleInstruction *IeleInstruction::CreateLoad(
    IeleLocalVariable *Result, IeleValue *AddressValue,
    IeleValue *OffsetValue, IeleValue *WidthValue,
    const SourceLocation &Loc, IeleInstruction *InsertBefore) {
  solAssert(Result, "CreateLoad: Invalid lvalues");
  solAssert(AddressValue && OffsetValue && WidthValue, "CreateLoad: Invalid operands");

  IeleInstruction *LoadInst = new IeleInstruction(Load, Loc, InsertBefore);
  LoadInst->getIeleLValueList().push_back(Result);
  LoadInst->getIeleOperandList().push_back(AddressValue);
  LoadInst->getIeleOperandList().push_back(OffsetValue);
  LoadInst->getIeleOperandList().push_back(WidthValue);

  return LoadInst;
}

IeleInstruction *IeleInstruction::CreateLoad(
    IeleLocalVariable *Result, IeleValue *AddressValue,
    IeleValue *OffsetValue, IeleValue *WidthValue,
    const SourceLocation &Loc, IeleBlock *InsertAtEnd) {
  solAssert(Result, "CreateLoad: Invalid lvalues");
  solAssert(AddressValue && OffsetValue && WidthValue, "CreateLoad: Invalid operands");

  IeleInstruction *LoadInst = new IeleInstruction(Load, Loc, InsertAtEnd);
  LoadInst->getIeleLValueList().push_back(Result);
  LoadInst->getIeleOperandList().push_back(AddressValue);
  LoadInst->getIeleOperandList().push_back(OffsetValue);
  LoadInst->getIeleOperandList().push_back(WidthValue);

  return LoadInst;
}


IeleInstruction *IeleInstruction::CreateStore(
    IeleValue *DataValue, IeleValue *AddressValue,
    const SourceLocation &Loc, IeleInstruction *InsertBefore) {
  solAssert(DataValue && AddressValue, "CreateStore: Invalid operands");

  IeleInstruction *StoreInst = new IeleInstruction(Store, Loc, InsertBefore);
  StoreInst->getIeleOperandList().push_back(DataValue);
  StoreInst->getIeleOperandList().push_back(AddressValue);

  return StoreInst;
}

IeleInstruction *IeleInstruction::CreateStore(
    IeleValue *DataValue, IeleValue *AddressValue, const SourceLocation &Loc, IeleBlock *InsertAtEnd) {
  solAssert(DataValue && AddressValue, "CreateStore: Invalid operands");

  IeleInstruction *StoreInst = new IeleInstruction(Store, Loc, InsertAtEnd);
  StoreInst->getIeleOperandList().push_back(DataValue);
  StoreInst->getIeleOperandList().push_back(AddressValue);

  return StoreInst;
}

// Same as CreateStore, but also accept OffsetValue and WidthValue as args. 
// Alternatively, we could also extend CreateStore by passing those as 
// optional args...
IeleInstruction *IeleInstruction::CreateStore(
    IeleValue *DataValue, IeleValue *AddressValue,
    IeleValue *OffsetValue, IeleValue *WidthValue, const SourceLocation &Loc, IeleInstruction *InsertBefore) {
  solAssert(DataValue && AddressValue, "CreateStore: Invalid operands");

  IeleInstruction *StoreInst = new IeleInstruction(Store, Loc, InsertBefore);
  StoreInst->getIeleOperandList().push_back(DataValue);
  StoreInst->getIeleOperandList().push_back(AddressValue);
  StoreInst->getIeleOperandList().push_back(OffsetValue);
  StoreInst->getIeleOperandList().push_back(WidthValue);

  return StoreInst;
}

IeleInstruction *IeleInstruction::CreateStore(
    IeleValue *DataValue, IeleValue *AddressValue,
    IeleValue *OffsetValue, IeleValue *WidthValue, const SourceLocation &Loc, IeleBlock *InsertAtEnd) {
  solAssert(DataValue && AddressValue, "CreateStore: Invalid operands");

  IeleInstruction *StoreInst = new IeleInstruction(Store, Loc, InsertAtEnd);
  StoreInst->getIeleOperandList().push_back(DataValue);
  StoreInst->getIeleOperandList().push_back(AddressValue);
  StoreInst->getIeleOperandList().push_back(OffsetValue);
  StoreInst->getIeleOperandList().push_back(WidthValue);

  return StoreInst;
}

IeleInstruction *IeleInstruction::CreateLog2(
    IeleLocalVariable *Result, IeleValue *OperandValue,
    const SourceLocation &Loc, IeleInstruction *InsertBefore) {
  solAssert(Result, "CreateBinOp: Invalid lvalues");
  solAssert(OperandValue,
         "CreateBinOp: Invalid operand");

  IeleInstruction *Log2Inst = new IeleInstruction(Log2, Loc, InsertBefore);
  Log2Inst->getIeleLValueList().push_back(Result);
  Log2Inst->getIeleOperandList().push_back(OperandValue);

  return Log2Inst; 
}

IeleInstruction *IeleInstruction::CreateLog2(
    IeleLocalVariable *Result, IeleValue *OperandValue,
    const SourceLocation &Loc, IeleBlock *InsertAtEnd) {
  solAssert(Result, "CreateBinOp: Invalid lvalues");
  solAssert(OperandValue,
         "CreateBinOp: Invalid operand");

  IeleInstruction *Log2Inst = new IeleInstruction(Log2, Loc, InsertAtEnd);
  Log2Inst->getIeleLValueList().push_back(Result);
  Log2Inst->getIeleOperandList().push_back(OperandValue);

  return Log2Inst;
}

IeleInstruction *IeleInstruction::CreateNot(
    IeleLocalVariable *Result, IeleValue *OperandValue,
    const SourceLocation &Loc, IeleInstruction *InsertBefore) {
  solAssert(Result, "CreateBinOp: Invalid lvalues");
  solAssert(OperandValue,
         "CreateBinOp: Invalid operand");

  IeleInstruction *NotInst = new IeleInstruction(Not, Loc, InsertBefore);
  NotInst->getIeleLValueList().push_back(Result);
  NotInst->getIeleOperandList().push_back(OperandValue);

  return NotInst;
}

IeleInstruction *IeleInstruction::CreateNot(
    IeleLocalVariable *Result, IeleValue *OperandValue,
    const SourceLocation &Loc, IeleBlock *InsertAtEnd) {
  solAssert(Result, "CreateBinOp: Invalid lvalues");
  solAssert(OperandValue,
         "CreateBinOp: Invalid operand");

  IeleInstruction *NotInst = new IeleInstruction(Not, Loc, InsertAtEnd);
  NotInst->getIeleLValueList().push_back(Result);
  NotInst->getIeleOperandList().push_back(OperandValue);

  return NotInst;
}

IeleInstruction *IeleInstruction::CreateSha3(
    IeleLocalVariable *Result, IeleValue *OperandValue,
    const SourceLocation &Loc, IeleInstruction *InsertBefore) {
  solAssert(Result, "CreateSha3: Invalid lvalues");
  solAssert(OperandValue, "CreateSha3: Invalid operand");

  IeleInstruction *Sha3Inst = new IeleInstruction(Sha3, Loc, InsertBefore);
  Sha3Inst->getIeleLValueList().push_back(Result);
  Sha3Inst->getIeleOperandList().push_back(OperandValue);

  return Sha3Inst;
}

IeleInstruction *IeleInstruction::CreateSha3(
    IeleLocalVariable *Result, IeleValue *OperandValue,
    const SourceLocation &Loc, IeleBlock *InsertAtEnd) {
  solAssert(Result, "CreateSha3: Invalid lvalues");
  solAssert(OperandValue, "CreateSha3: Invalid operand");

  IeleInstruction *Sha3Inst = new IeleInstruction(Sha3, Loc, InsertAtEnd);
  Sha3Inst->getIeleLValueList().push_back(Result);
  Sha3Inst->getIeleOperandList().push_back(OperandValue);

  return Sha3Inst;
}

IeleInstruction *IeleInstruction::CreateBinOp(
    IeleOps BinOpcode, IeleLocalVariable *Result, IeleValue *LeftOperandValue,
    IeleValue *RightOperandValue, const SourceLocation &Loc, IeleInstruction *InsertBefore) {
  solAssert(Result, "CreateBinOp: Invalid lvalues");
  solAssert(LeftOperandValue && RightOperandValue,
         "CreateBinOp: Invalid operands");

  IeleInstruction *BinOpInst = new IeleInstruction(BinOpcode, Loc, InsertBefore);
  BinOpInst->getIeleLValueList().push_back(Result);
  BinOpInst->getIeleOperandList().push_back(LeftOperandValue);
  BinOpInst->getIeleOperandList().push_back(RightOperandValue);

  return BinOpInst;
}

IeleInstruction *IeleInstruction::CreateBinOp(
    IeleOps BinOpcode, IeleLocalVariable *Result, IeleValue *LeftOperandValue,
    IeleValue *RightOperandValue, const SourceLocation &Loc, IeleBlock *InsertAtEnd) {
  solAssert(Result, "CreateBinOp: Invalid lvalues");
  solAssert(LeftOperandValue && RightOperandValue,
         "CreateBinOp: Invalid operands");

  IeleInstruction *BinOpInst = new IeleInstruction(BinOpcode, Loc, InsertAtEnd);
  BinOpInst->getIeleLValueList().push_back(Result);
  BinOpInst->getIeleOperandList().push_back(LeftOperandValue);
  BinOpInst->getIeleOperandList().push_back(RightOperandValue);

  return BinOpInst;
}

IeleInstruction *IeleInstruction::CreateTernOp(
    IeleOps TernOpcode, IeleLocalVariable *Result, IeleValue *FirstOperandValue,
    IeleValue *SecondOperandValue, IeleValue *ThirdOperandValue,
    const SourceLocation &Loc, IeleInstruction *InsertBefore) {
  solAssert(Result, "CreateTernOp: Invalid lvalues");
  solAssert(FirstOperandValue && SecondOperandValue && ThirdOperandValue,
         "CreateTernOp: Invalid operands");

  IeleInstruction *TernOpInst = new IeleInstruction(TernOpcode, Loc, InsertBefore);
  TernOpInst->getIeleLValueList().push_back(Result);
  TernOpInst->getIeleOperandList().push_back(FirstOperandValue);
  TernOpInst->getIeleOperandList().push_back(SecondOperandValue);
  TernOpInst->getIeleOperandList().push_back(ThirdOperandValue);

  return TernOpInst;
}

IeleInstruction *IeleInstruction::CreateTernOp(
    IeleOps TernOpcode, IeleLocalVariable *Result, IeleValue *FirstOperandValue,
    IeleValue *SecondOperandValue, IeleValue *ThirdOperandValue,
    const SourceLocation &Loc, IeleBlock *InsertAtEnd) {
  solAssert(Result, "CreateTernOp: Invalid lvalues");
  solAssert(FirstOperandValue && SecondOperandValue && ThirdOperandValue,
         "CreateTernOp: Invalid operands");

  IeleInstruction *TernOpInst = new IeleInstruction(TernOpcode, Loc, InsertAtEnd);
  TernOpInst->getIeleLValueList().push_back(Result);
  TernOpInst->getIeleOperandList().push_back(FirstOperandValue);
  TernOpInst->getIeleOperandList().push_back(SecondOperandValue);
  TernOpInst->getIeleOperandList().push_back(ThirdOperandValue);

  return TernOpInst;
}

static void printOpcode(llvm::raw_ostream &OS, const IeleInstruction *I) {
  switch (I->getOpcode()) {
#define HANDLE_IELE_INST(N, OPC, TXT) \
  case IeleInstruction::OPC: OS << TXT; break;
#include "IeleInstruction.def"

  default:
    solAssert(false, "unreachable");
  }
}

static void printCall(llvm::raw_ostream &OS, const IeleInstruction *I) {
  // print lvalues
  bool isFirst = true;
  for (auto It = I->lvalue_begin(); It != I->lvalue_end(); ++It) {
    if (!isFirst)
      OS << ", ";
    (*It)->printAsValue(OS);
    isFirst = false;
  }

  // print opcode
  if (I->lvalue_size())
    OS << " = ";
  printOpcode(OS, I);
  OS << " ";

  // print call target
  auto It = I->begin();
  (*It)->printAsValue(OS); ++It;

  // in case of account calls, print called account
  if (I->getOpcode() == IeleInstruction::CallAt ||
      I->getOpcode() == IeleInstruction::StaticCallAt ||
      I->getOpcode() == IeleInstruction::CallAddress) {
    OS << " at ";
    (*It)->printAsValue(OS);
    ++It;
    if (I->getOpcode() == IeleInstruction::CallAddress)
      return;
    OS << " ";
  }

  // Skip send and gaslimit arguments, in case of creation or account call
  auto SavedIt = It;
  if (I->getOpcode() != IeleInstruction::Call)
    ++It;
  if (I->getOpcode() == IeleInstruction::CallAt)
    ++It;

  // print operands
  OS << "(";
  isFirst = true;
  for (; It != I->end(); ++It) {
    if (!isFirst)
      OS << ", ";
    (*It)->printAsValue(OS);
    isFirst = false;
  }
  OS << ")";

  // print send
  if (I->getOpcode() != IeleInstruction::Call &&
      I->getOpcode() != IeleInstruction::StaticCallAt) {
    OS << " send ";
    (*SavedIt)->printAsValue(OS);
    ++SavedIt;
  }
  
  // print gaslimit
  if (I->getOpcode() == IeleInstruction::CallAt)
    OS << ",";
  if (I->getOpcode() == IeleInstruction::CallAt ||
      I->getOpcode() == IeleInstruction::StaticCallAt) {
    OS << " gaslimit ";
    (*SavedIt)->printAsValue(OS);
  }
}

static void printIntrinsic(llvm::raw_ostream &OS, const IeleInstruction *I) {
  // print lvalue
  if (!I->getIeleLValueList().empty()) {
    (*I->lvalue_begin())->printAsValue(OS);
    // print opcode
    OS << " = call ";
  } else {
    OS << "call ";
  }

  printOpcode(OS, I);

  // print operands
  OS << "(";
  bool isFirst = true;
  for (auto It = I->begin(); It != I->end(); ++It) {
    if (!isFirst)
      OS << ", ";
    (*It)->printAsValue(OS);
    isFirst = false;
  }
  OS << ")";
}

static void printOtherInst(llvm::raw_ostream &OS, const IeleInstruction *I) {
  // print lvalues
  bool isFirst = true;
  for (auto It = I->lvalue_begin(); It != I->lvalue_end(); ++It) {
    if (!isFirst)
      OS << ", ";
    (*It)->printAsValue(OS);
    isFirst = false;
  }

  // print opcode
  if (I->lvalue_size())
    OS << " = ";
  printOpcode(OS, I);

  // print operands
  isFirst = true;
  for (auto It = I->begin(); It != I->end(); ++It) {
    if (!isFirst)
      OS << ", ";
    else if (I->getOpcode() != IeleInstruction::Assign)
      OS << " ";
    (*It)->printAsValue(OS);
    isFirst = false;
  }

  // In case of ret, check for no operands
  if (I->getOpcode() == IeleInstruction::Ret && I->size() == 0)
    OS << " void";
}

void IeleInstruction::print(llvm::raw_ostream &OS, unsigned indent) const {
  std::string Indent(indent, ' ');
  OS << Indent;

  switch (getOpcode()) {
#define HANDLE_IELE_OTHER_INST(N, OPC, TXT) \
  case OPC: printOtherInst(OS, this); break;
#include "IeleInstruction.def"

#define HANDLE_IELE_INTRINSIC_INST(N, OPC, TXT) \
  case OPC: printIntrinsic(OS, this); break;
#include "IeleInstruction.def"

#define HANDLE_IELE_CALL_INST(N, OPC, TXT) \
  case OPC: printCall(OS, this); break;
#include "IeleInstruction.def"

  default:
    solAssert(false, "unreachable");
  }
}


