#include "libsolidity/codegen/IeleLValue.h"

#include "libiele/IeleIntConstant.h"

using namespace dev;
using namespace dev::solidity;

RegisterLValue::RegisterLValue(std::vector<iele::IeleLocalVariable *> Var) :
  Var(Var) {}

void RegisterLValue::write(IeleRValue *Value, iele::IeleBlock *InsertAtEnd) const {
  solAssert(Value->getValues().size() == Var.size(), "Invalid number of rvalues");
  for (unsigned i = 0; i < Var.size(); i++) {
    iele::IeleInstruction::CreateAssign(Var[i], Value->getValues()[i], InsertAtEnd);
  }
}

IeleRValue *RegisterLValue::read(iele::IeleBlock *InsertAtEnd) const {
  std::vector<iele::IeleValue *> Result;
  Result.insert(Result.begin(), Var.begin(), Var.end());
  return IeleRValue::Create(Result);
}

AddressLValue::AddressLValue(IeleCompiler *Compiler, iele::IeleValue *Address, DataLocation Loc, std::string Name, unsigned Num) :
  Compiler(Compiler), Address(Address), Loc(Loc), Name(Name), Num(Num) {}

void AddressLValue::write(IeleRValue *Value, iele::IeleBlock *InsertAtEnd) const {
  solAssert(Value->getValues().size() == Num, "Invalid number of rvalues");
  for (unsigned i = 0; i < Num; i++) {
    iele::IeleValue *ElementRValue;
    if (i == 0) {
      ElementRValue = Address;
    } else {
      iele::IeleLocalVariable *Element = iele::IeleLocalVariable::Create(&Compiler->Context, Name + ".address", Compiler->CompilingFunction);
      ElementRValue = Element;
      iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Add, Element, Address, iele::IeleIntConstant::Create(&Compiler->Context, i),
        InsertAtEnd);
    }
    if (Loc == DataLocation::Storage)
      iele::IeleInstruction::CreateSStore(Value->getValues()[i], ElementRValue, InsertAtEnd);
    else
      iele::IeleInstruction::CreateStore(Value->getValues()[i], ElementRValue, InsertAtEnd);
  }
}

IeleRValue *AddressLValue::read(iele::IeleBlock *InsertAtEnd) const {
  std::vector<iele::IeleValue *> Results;
  for (unsigned i = 0; i < Num; i++) {
    iele::IeleLocalVariable *Result = iele::IeleLocalVariable::Create(&Compiler->Context, Name + ".val", Compiler->CompilingFunction);
    Results.push_back(Result);
    iele::IeleValue *ElementRValue;
    if (i == 0) {
      ElementRValue = Address;
    } else {
      iele::IeleLocalVariable *Element = iele::IeleLocalVariable::Create(&Compiler->Context, Name + ".address", Compiler->CompilingFunction);
      ElementRValue = Element;
      iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Add, Element, Address, iele::IeleIntConstant::Create(&Compiler->Context, i),
        InsertAtEnd);
    }
    if (Loc == DataLocation::Storage)
      iele::IeleInstruction::CreateSLoad(Result, ElementRValue, InsertAtEnd);
    else
      iele::IeleInstruction::CreateLoad(Result, ElementRValue, InsertAtEnd);
  }
  return IeleRValue::Create(Results);
}

ByteArrayLValue::ByteArrayLValue(IeleCompiler *Compiler, iele::IeleValue *Address, iele::IeleValue *Offset, DataLocation Loc) :
  Compiler(Compiler), Address(Address), Offset(Offset), Loc(Loc) {}

void ByteArrayLValue::write(IeleRValue *Value, iele::IeleBlock *InsertAtEnd) const {
  if (Loc == DataLocation::Storage) {
    iele::IeleLocalVariable *OldValue = iele::IeleLocalVariable::Create(&Compiler->Context, "string.val", Compiler->CompilingFunction);
    iele::IeleInstruction::CreateSLoad(OldValue, Address, InsertAtEnd);
    iele::IeleValue *Spill = Compiler->appendMemorySpill();
    iele::IeleInstruction::CreateStore(OldValue, Spill, InsertAtEnd);
    iele::IeleInstruction::CreateStore(Value->getValue(), Spill, Offset, iele::IeleIntConstant::getOne(&Compiler->Context), InsertAtEnd);
    iele::IeleInstruction::CreateLoad(OldValue, Spill, InsertAtEnd);
    iele::IeleInstruction::CreateSStore(OldValue, Address, InsertAtEnd);
  } else
    iele::IeleInstruction::CreateStore(Value->getValue(), Address, Offset, iele::IeleIntConstant::getOne(&Compiler->Context), InsertAtEnd);
}

IeleRValue *ByteArrayLValue::read(iele::IeleBlock *InsertAtEnd) const {
  iele::IeleLocalVariable *Result = iele::IeleLocalVariable::Create(&Compiler->Context , "string.val", Compiler->CompilingFunction);
  if (Loc == DataLocation::Storage) {
    iele::IeleInstruction::CreateSLoad(Result, Address, InsertAtEnd);
    iele::IeleInstruction::CreateBinOp(iele::IeleInstruction::Byte, Result, Offset, Result, InsertAtEnd);
  } else {
    iele::IeleInstruction::CreateLoad(Result, Address, Offset, iele::IeleIntConstant::getOne(&Compiler->Context), InsertAtEnd);
    iele::IeleInstruction::CreateBinOp(iele::IeleInstruction::Byte, Result, iele::IeleIntConstant::getZero(&Compiler->Context), Result, InsertAtEnd);
  }
  return IeleRValue::Create({Result});
}

void ArrayLengthLValue::write(IeleRValue *Value, iele::IeleBlock *InsertAtEnd) const {
  solAssert(Loc == DataLocation::Storage, "invalid location for array length write");
  Compiler->appendArrayLengthResize(Address, Value->getValue());
  AddressLValue::write(Value, Compiler->CompilingBlock);
}
