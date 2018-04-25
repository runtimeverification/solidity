#include "libsolidity/codegen/IeleLValue.h"

#include "libiele/IeleIntConstant.h"

using namespace dev;
using namespace dev::solidity;

RegisterLValue::RegisterLValue(iele::IeleLocalVariable *Var) :
  Var(Var) {}

void RegisterLValue::write(iele::IeleValue *Value, iele::IeleBlock *InsertAtEnd) const {
  iele::IeleInstruction::CreateAssign(Var, Value, InsertAtEnd);
}

iele::IeleValue *RegisterLValue::read(iele::IeleBlock *InsertAtEnd) const {
  return Var;
}

AddressLValue::AddressLValue(IeleCompiler *Compiler, iele::IeleValue *Address, DataLocation Loc, std::string Name) :
  Compiler(Compiler), Address(Address), Loc(Loc), Name(Name) {}

void AddressLValue::write(iele::IeleValue *Value, iele::IeleBlock *InsertAtEnd) const {
  if (Loc == DataLocation::Storage)
    iele::IeleInstruction::CreateSStore(Value, Address, InsertAtEnd);
  else
    iele::IeleInstruction::CreateStore(Value, Address, InsertAtEnd);
}

iele::IeleValue *AddressLValue::read(iele::IeleBlock *InsertAtEnd) const {
  iele::IeleLocalVariable *Result = iele::IeleLocalVariable::Create(&Compiler->Context, Name + ".val", Compiler->CompilingFunction);
  if (Loc == DataLocation::Storage)
    iele::IeleInstruction::CreateSLoad(Result, Address, InsertAtEnd);
  else
    iele::IeleInstruction::CreateLoad(Result, Address, InsertAtEnd);
  return Result;
}

ByteArrayLValue::ByteArrayLValue(IeleCompiler *Compiler, iele::IeleValue *Address, iele::IeleValue *Offset, DataLocation Loc) :
  Compiler(Compiler), Address(Address), Offset(Offset), Loc(Loc) {}

void ByteArrayLValue::write(iele::IeleValue *Value, iele::IeleBlock *InsertAtEnd) const {
  if (Loc == DataLocation::Storage) {
    iele::IeleLocalVariable *OldValue = iele::IeleLocalVariable::Create(&Compiler->Context, "string.val", Compiler->CompilingFunction);
    iele::IeleInstruction::CreateSLoad(OldValue, Address, InsertAtEnd);
    iele::IeleValue *Spill = Compiler->appendMemorySpill();
    iele::IeleInstruction::CreateStore(OldValue, Spill, InsertAtEnd);
    iele::IeleInstruction::CreateStore(Value, Spill, Offset, iele::IeleIntConstant::getOne(&Compiler->Context), InsertAtEnd);
    iele::IeleInstruction::CreateLoad(OldValue, Spill, InsertAtEnd);
    iele::IeleInstruction::CreateSStore(OldValue, Address, InsertAtEnd);
  } else
    iele::IeleInstruction::CreateStore(Value, Address, Offset, iele::IeleIntConstant::getOne(&Compiler->Context), InsertAtEnd);
}

iele::IeleValue *ByteArrayLValue::read(iele::IeleBlock *InsertAtEnd) const {
  iele::IeleLocalVariable *Result = iele::IeleLocalVariable::Create(&Compiler->Context , "string.val", Compiler->CompilingFunction);
  if (Loc == DataLocation::Storage) {
    iele::IeleInstruction::CreateSLoad(Result, Address, InsertAtEnd);
    iele::IeleInstruction::CreateBinOp(iele::IeleInstruction::Byte, Result, Offset, Result, InsertAtEnd);
  } else {
    iele::IeleInstruction::CreateLoad(Result, Address, Offset, iele::IeleIntConstant::getOne(&Compiler->Context), InsertAtEnd);
    iele::IeleInstruction::CreateBinOp(iele::IeleInstruction::Byte, Result, iele::IeleIntConstant::getZero(&Compiler->Context), Result, InsertAtEnd);
  }
  return Result;
}

void ArrayLengthLValue::write(iele::IeleValue *Value, iele::IeleBlock *InsertAtEnd) const {
  solAssert(Loc == DataLocation::Storage, "invalid location for array length write");
  Compiler->appendArrayLengthResize(Address, Value);
  AddressLValue::write(Value, Compiler->CompilingBlock);
}
