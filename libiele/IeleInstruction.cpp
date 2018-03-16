#include "IeleInstruction.h"

#include "IeleFunction.h"
#include "IeleGlobalVariable.h"

#include "llvm/Support/raw_ostream.h"

using namespace dev;
using namespace dev::iele;

IeleInstruction::IeleInstruction(IeleOps opc, IeleInstruction *InsertBefore) :
  InstID(opc), Parent(nullptr) {
  // If requested, insert this instruction into a IeleBlock.
  if (InsertBefore) {
    IeleBlock *B = InsertBefore->getParent();
    assert(B && "Instruction to insert before is not in a block!");
    B->getIeleInstructionList().insert(InsertBefore->getIterator(), this);
  }
}

IeleInstruction::IeleInstruction(IeleOps opc, IeleBlock *InsertAtEnd) :
  InstID(opc), Parent(nullptr) {
  // Append this instruction into the given IeleBlock.
  assert(InsertAtEnd && "Block to append to may not be NULL!");
  InsertAtEnd->getIeleInstructionList().push_back(this);
}

void IeleInstruction::setParent(IeleBlock *parent) {
  // Assert that all operands and lvalues have the same parent as the block.
  IeleFunction *F = parent->getParent();
  for (const IeleValue *V : operands()) {
    if (const IeleLocalVariable *LV = llvm::dyn_cast<IeleLocalVariable>(V))
      assert(LV->getParent() == F &&
             "Instruction operand belongs to a different function!");
    else if (const IeleBlock *B = llvm::dyn_cast<IeleBlock>(V))
      assert(B->getParent() == F &&
             "Instruction operand belongs to a different function!");
  }

  for (const IeleLocalVariable *LV : lvalues()) {
    assert(LV->getParent() == F &&
           "Instruction lvalue belongs to a different function!");
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

IeleInstruction *IeleInstruction::CreateRetVoid(IeleInstruction *InsertBefore) {
  return new IeleInstruction(Ret, InsertBefore);
}

IeleInstruction *IeleInstruction::CreateRetVoid(IeleBlock *InsertAtEnd) {
  return new IeleInstruction(Ret, InsertAtEnd);
}

IeleInstruction *IeleInstruction::CreateRet(
    llvm::SmallVectorImpl<IeleValue *> &ReturnValues,
    IeleInstruction *InsertBefore) {
  assert(ReturnValues.size() > 0 && "CreateRet: Invalid operands");

  IeleInstruction *RetInst = new IeleInstruction(Ret, InsertBefore);
  RetInst->getIeleOperandList().insert(RetInst->end(), ReturnValues.begin(), 
                                       ReturnValues.end());

  return RetInst;
}

IeleInstruction *IeleInstruction::CreateRet(
    llvm::SmallVectorImpl<IeleValue *> &ReturnValues, IeleBlock *InsertAtEnd) {
  assert(ReturnValues.size() > 0 && "CreateRet: Invalid operands");

  IeleInstruction *RetInst = new IeleInstruction(Ret, InsertAtEnd);
  RetInst->getIeleOperandList().insert(RetInst->end(), ReturnValues.begin(),
                                       ReturnValues.end());

  return RetInst;
}

IeleInstruction *IeleInstruction::CreateRevert(
    IeleValue * StatusValue, IeleInstruction *InsertBefore) {
  assert(StatusValue && "CreateRevert: Invalid operands");

  IeleInstruction *RevertInst = new IeleInstruction(Revert, InsertBefore);
  RevertInst->getIeleOperandList().push_back(StatusValue);

  return RevertInst;
}

IeleInstruction *IeleInstruction::CreateRevert(
    IeleValue *StatusValue, IeleBlock *InsertAtEnd) {
  assert(StatusValue && "CreateRevert: Invalid operands");

  IeleInstruction *RevertInst = new IeleInstruction(Revert, InsertAtEnd);
  RevertInst->getIeleOperandList().push_back(StatusValue);

  return RevertInst;
}

IeleInstruction *IeleInstruction::CreateIsZero(
    IeleLocalVariable *Result, IeleValue *ConditionValue,
    IeleInstruction *InsertBefore) {
  assert(Result && "CreateIsZero: Invalid lvalues");
  assert(ConditionValue && "CreateIsZero: Invalid operands");

  IeleInstruction *IsZeroInst = new IeleInstruction(IsZero, InsertBefore);
  IsZeroInst->getIeleLValueList().push_back(Result);
  IsZeroInst->getIeleOperandList().push_back(ConditionValue);

  return IsZeroInst;
}

IeleInstruction *IeleInstruction::CreateIsZero(
    IeleLocalVariable *Result, IeleValue *ConditionValue,
    IeleBlock *InsertAtEnd) {
  assert(Result && "CreateIsZero: Invalid lvalues");
  assert(ConditionValue && "CreateIsZero: Invalid operands");

  IeleInstruction *IsZeroInst = new IeleInstruction(IsZero, InsertAtEnd);
  IsZeroInst->getIeleLValueList().push_back(Result);
  IsZeroInst->getIeleOperandList().push_back(ConditionValue);

  return IsZeroInst;
}

IeleInstruction *IeleInstruction::CreateUncondBr(
    IeleBlock *Target, IeleInstruction *InsertBefore) {
  assert(Target && "CreateUncondBr: Invalid operands");

  IeleInstruction *UncondBrInst = new IeleInstruction(Br, InsertBefore);
  UncondBrInst->getIeleOperandList().push_back(Target);

  return UncondBrInst;
}

IeleInstruction *IeleInstruction::CreateUncondBr(
    IeleBlock *Target, IeleBlock *InsertAtEnd) {
  assert(Target && "CreateUncondBr: Invalid operands");

  IeleInstruction *UncondBrInst = new IeleInstruction(Br, InsertAtEnd);
  UncondBrInst->getIeleOperandList().push_back(Target);

  return UncondBrInst;
}

IeleInstruction *IeleInstruction::CreateCondBr(
    IeleValue *Condition, IeleBlock *Target, IeleInstruction *InsertBefore) {
  assert(Condition && Target && "CreateCondBr: Invalid operands");

  IeleInstruction *CondBrInst = new IeleInstruction(Br, InsertBefore);
  CondBrInst->getIeleOperandList().push_back(Condition);
  CondBrInst->getIeleOperandList().push_back(Target);

  return CondBrInst;
}

IeleInstruction *IeleInstruction::CreateCondBr(
    IeleValue *Condition, IeleBlock *Target, IeleBlock *InsertAtEnd) {
  assert(Condition && Target && "CreateCondBr: Invalid operands");

  IeleInstruction *CondBrInst = new IeleInstruction(Br, InsertAtEnd);
  CondBrInst->getIeleOperandList().push_back(Condition);
  CondBrInst->getIeleOperandList().push_back(Target);

  return CondBrInst;
}

IeleInstruction *IeleInstruction::CreateAccountCall(
    IeleLocalVariable *StatusValue, IeleGlobalVariable *Callee,
    IeleValue *AddressValue, IeleValue *TransferValue, IeleValue *GasValue,
    llvm::SmallVectorImpl<IeleValue *> &ArgumentValues,
    IeleInstruction *InsertBefore) {
  assert(Callee && AddressValue && TransferValue && GasValue &&
         "CreateAccountCall: Invalid operands");

  IeleInstruction *AccountCallInst = new IeleInstruction(CallAt, InsertBefore);
  if (StatusValue)
    AccountCallInst->getIeleLValueList().push_back(StatusValue);
  AccountCallInst->getIeleOperandList().push_back(Callee);
  AccountCallInst->getIeleOperandList().push_back(AddressValue);
  AccountCallInst->getIeleOperandList().push_back(TransferValue);
  AccountCallInst->getIeleOperandList().push_back(GasValue);
  AccountCallInst->getIeleOperandList().insert(AccountCallInst->end(),
                                        ArgumentValues.begin(),
                                        ArgumentValues.end());

  return AccountCallInst;
}

IeleInstruction *IeleInstruction::CreateAccountCall(
    IeleLocalVariable *StatusValue, IeleGlobalVariable *Callee,
    IeleValue *AddressValue, IeleValue *TransferValue, IeleValue *GasValue,
    llvm::SmallVectorImpl<IeleValue *> &ArgumentValues,
    IeleBlock *InsertAtEnd) {
  assert(Callee && AddressValue && TransferValue && GasValue &&
         "CreateAccountCall: Invalid operands");

  IeleInstruction *AccountCallInst = new IeleInstruction(CallAt, InsertAtEnd);
  if (StatusValue)
    AccountCallInst->getIeleLValueList().push_back(StatusValue);
  AccountCallInst->getIeleOperandList().push_back(Callee);
  AccountCallInst->getIeleOperandList().push_back(AddressValue);
  AccountCallInst->getIeleOperandList().push_back(TransferValue);
  AccountCallInst->getIeleOperandList().push_back(GasValue);
  AccountCallInst->getIeleOperandList().insert(AccountCallInst->end(),
                                               ArgumentValues.begin(),
                                               ArgumentValues.end());

  return AccountCallInst;
}

IeleInstruction *IeleInstruction::CreateIntrinsicCall(
    IeleOps IntrinsicOpcode, IeleLocalVariable *Result,
    llvm::SmallVectorImpl<IeleValue *> &ArgumentValues,
    IeleInstruction *InsertBefore) {
  //assert(IeleOps >= FIRST
  //       "CreateIntrinsicCall: Invalid opcode");

  IeleInstruction *IntrinsicCallInst =
    new IeleInstruction(IntrinsicOpcode, InsertBefore);
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
    IeleBlock *InsertAtEnd) {
  IeleInstruction *IntrinsicCallInst =
    new IeleInstruction(IntrinsicOpcode, InsertAtEnd);
  if (Result)
    IntrinsicCallInst->getIeleLValueList().push_back(Result);
  IntrinsicCallInst->getIeleOperandList().insert(IntrinsicCallInst->end(),
                                                 ArgumentValues.begin(),
                                                 ArgumentValues.end());

  return IntrinsicCallInst;
}

IeleInstruction *IeleInstruction::CreateAssign(
    IeleLocalVariable *Result, IeleValue *RHSValue,
    IeleInstruction *InsertBefore ) {
  assert(Result && "CreateAssign: Invalid lvalues");
  assert(RHSValue && "CreateAssign: Invalid operands");

  IeleInstruction *AssignInst = new IeleInstruction(Assign, InsertBefore);
  AssignInst->getIeleLValueList().push_back(Result);
  AssignInst->getIeleOperandList().push_back(RHSValue);

  return AssignInst;
}

IeleInstruction *IeleInstruction::CreateAssign(
    IeleLocalVariable *Result, IeleValue *RHSValue,
    IeleBlock *InsertAtEnd) {
  assert(Result && "CreateAssign: Invalid lvalues");
  assert(RHSValue && "CreateAssign: Invalid operands");

  IeleInstruction *AssignInst = new IeleInstruction(Assign, InsertAtEnd);
  AssignInst->getIeleLValueList().push_back(Result);
  AssignInst->getIeleOperandList().push_back(RHSValue);

  return AssignInst;
}

IeleInstruction *IeleInstruction::CreateSLoad(
    IeleLocalVariable *Result, IeleValue *AddressValue,
    IeleInstruction *InsertBefore) {
  assert(Result && "CreateSLoad: Invalid lvalues");
  assert(AddressValue && "CreateSLoad: Invalid operands");

  IeleInstruction *SLoadInst = new IeleInstruction(SLoad, InsertBefore);
  SLoadInst->getIeleLValueList().push_back(Result);
  SLoadInst->getIeleOperandList().push_back(AddressValue);

  return SLoadInst;
}

IeleInstruction *IeleInstruction::CreateSLoad(
    IeleLocalVariable *Result, IeleValue *AddressValue,
    IeleBlock *InsertAtEnd) {
  assert(Result && "CreateSLoad: Invalid lvalues");
  assert(AddressValue && "CreateSLoad: Invalid operands");

  IeleInstruction *SLoadInst = new IeleInstruction(SLoad, InsertAtEnd);
  SLoadInst->getIeleLValueList().push_back(Result);
  SLoadInst->getIeleOperandList().push_back(AddressValue);

  return SLoadInst;
}

IeleInstruction *IeleInstruction::CreateSStore(
    IeleValue *DataValue, IeleValue *AddressValue,
    IeleInstruction *InsertBefore) {
  assert(DataValue && AddressValue && "CreateSStore: Invalid operands");

  IeleInstruction *SStoreInst = new IeleInstruction(SStore, InsertBefore);
  SStoreInst->getIeleOperandList().push_back(DataValue);
  SStoreInst->getIeleOperandList().push_back(AddressValue);

  return SStoreInst;
}

IeleInstruction *IeleInstruction::CreateSStore(
    IeleValue *DataValue, IeleValue *AddressValue, IeleBlock *InsertAtEnd) {
  assert(DataValue && AddressValue && "CreateSStore: Invalid operands");

  IeleInstruction *SStoreInst = new IeleInstruction(SStore, InsertAtEnd);
  SStoreInst->getIeleOperandList().push_back(DataValue);
  SStoreInst->getIeleOperandList().push_back(AddressValue);

  return SStoreInst;
}

IeleInstruction *IeleInstruction::CreateBinOp(
    IeleOps BinOpcode, IeleLocalVariable *Result, IeleValue *LeftOperandValue,
    IeleValue *RightOperandValue, IeleInstruction *InsertBefore) {
  assert(Result && "CreateBinOp: Invalid lvalues");
  assert(LeftOperandValue && RightOperandValue &&
         "CreateBinOp: Invalid operands");

  IeleInstruction *BinOpInst = new IeleInstruction(BinOpcode, InsertBefore);
  BinOpInst->getIeleLValueList().push_back(Result);
  BinOpInst->getIeleOperandList().push_back(LeftOperandValue);
  BinOpInst->getIeleOperandList().push_back(RightOperandValue);

  return BinOpInst;
}

IeleInstruction *IeleInstruction::CreateBinOp(
    IeleOps BinOpcode, IeleLocalVariable *Result, IeleValue *LeftOperandValue,
    IeleValue *RightOperandValue, IeleBlock *InsertAtEnd) {
  assert(Result && "CreateBinOp: Invalid lvalues");
  assert(LeftOperandValue && RightOperandValue &&
         "CreateBinOp: Invalid operands");

  IeleInstruction *BinOpInst = new IeleInstruction(BinOpcode, InsertAtEnd);
  BinOpInst->getIeleLValueList().push_back(Result);
  BinOpInst->getIeleOperandList().push_back(LeftOperandValue);
  BinOpInst->getIeleOperandList().push_back(RightOperandValue);

  return BinOpInst;
}

IeleInstruction *IeleInstruction::CreateTernOp(
    IeleOps TernOpcode, IeleLocalVariable *Result, IeleValue *FirstOperandValue,
    IeleValue *SecondOperandValue, IeleValue *ThirdOperandValue,
    IeleInstruction *InsertBefore) {
  assert(Result && "CreateTernOp: Invalid lvalues");
  assert(FirstOperandValue && SecondOperandValue && ThirdOperandValue &&
         "CreateTernOp: Invalid operands");

  IeleInstruction *TernOpInst = new IeleInstruction(TernOpcode, InsertBefore);
  TernOpInst->getIeleLValueList().push_back(Result);
  TernOpInst->getIeleOperandList().push_back(FirstOperandValue);
  TernOpInst->getIeleOperandList().push_back(SecondOperandValue);
  TernOpInst->getIeleOperandList().push_back(ThirdOperandValue);

  return TernOpInst;
}

IeleInstruction *IeleInstruction::CreateTernOp(
    IeleOps TernOpcode, IeleLocalVariable *Result, IeleValue *FirstOperandValue,
    IeleValue *SecondOperandValue, IeleValue *ThirdOperandValue,
    IeleBlock *InsertAtEnd) {
  assert(Result && "CreateTernOp: Invalid lvalues");
  assert(FirstOperandValue && SecondOperandValue && ThirdOperandValue &&
         "CreateTernOp: Invalid operands");

  IeleInstruction *TernOpInst = new IeleInstruction(TernOpcode, InsertAtEnd);
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
    assert(false && "unreachable");
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
      I->getOpcode() == IeleInstruction::StaticCallAt) {
    OS << " at ";
    (*It)->printAsValue(OS);
    ++It;
    OS << " ";
  }

  // Skip send and gaslimit arguments, in case of creation or account call
  auto SavedIt = It;
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
  (*I->lvalue_begin())->printAsValue(OS);

  // print opcode
  OS << " = call ";
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
    assert(false && "unreachable");
  }
}


