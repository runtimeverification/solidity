#include "libsolidity/codegen/IeleLValue.h"

#include "libiele/IeleIntConstant.h"

using namespace solidity;
using namespace solidity::frontend;
using namespace solidity::langutil;

RegisterLValue::RegisterLValue(std::vector<iele::IeleLocalVariable *> Var) :
  Var(Var) {}

void RegisterLValue::write(IeleRValue *Value, const SourceLocation &SLoc, iele::IeleBlock *InsertAtEnd) const {
  solAssert(Value->getValues().size() == Var.size(), "Invalid number of rvalues");
  for (unsigned i = 0; i < Var.size(); i++) {
    iele::IeleInstruction::CreateAssign(Var[i], Value->getValues()[i], SLoc, InsertAtEnd);
  }
}

IeleRValue *RegisterLValue::read(const SourceLocation &SLoc, iele::IeleBlock *InsertAtEnd) const {
  std::vector<iele::IeleValue *> Result;
  Result.insert(Result.begin(), Var.begin(), Var.end());
  return IeleRValue::Create(Result);
}

AddressLValue::AddressLValue(IeleCompiler *Compiler, iele::IeleValue *Address, DataLocation Loc, std::string Name, unsigned Num) :
  Compiler(Compiler), Address(Address), Loc(Loc), Name(Name), Num(Num) {}

void AddressLValue::write(IeleRValue *Value, const SourceLocation &SLoc, iele::IeleBlock *InsertAtEnd) const {
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
        SLoc, InsertAtEnd);
    }
    if (Loc == DataLocation::Storage)
      iele::IeleInstruction::CreateSStore(Value->getValues()[i], ElementRValue, SLoc, InsertAtEnd);
    else
      iele::IeleInstruction::CreateStore(Value->getValues()[i], ElementRValue, SLoc, InsertAtEnd);
  }
}

IeleRValue *AddressLValue::read(const SourceLocation &SLoc, iele::IeleBlock *InsertAtEnd) const {
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
        SLoc, InsertAtEnd);
    }
    if (Loc == DataLocation::Storage)
      iele::IeleInstruction::CreateSLoad(Result, ElementRValue, SLoc, InsertAtEnd);
    else
      iele::IeleInstruction::CreateLoad(Result, ElementRValue, SLoc, InsertAtEnd);
  }
  return IeleRValue::Create(Results);
}

ByteArrayLValue::ByteArrayLValue(IeleCompiler *Compiler, iele::IeleValue *Address, iele::IeleValue *Offset, DataLocation Loc) :
  Compiler(Compiler), Address(Address), Offset(Offset), Loc(Loc) {}

void ByteArrayLValue::write(IeleRValue *Value, const SourceLocation &SLoc, iele::IeleBlock *InsertAtEnd) const {
  if (Loc == DataLocation::Storage) {
    iele::IeleLocalVariable *OldValue = iele::IeleLocalVariable::Create(&Compiler->Context, "string.val", Compiler->CompilingFunction);
    iele::IeleInstruction::CreateSLoad(OldValue, Address, SLoc, InsertAtEnd);
    iele::IeleValue *Spill = Compiler->appendMemorySpill();
    iele::IeleInstruction::CreateStore(OldValue, Spill, SLoc, InsertAtEnd);
    iele::IeleInstruction::CreateStore(Value->getValue(), Spill, Offset, iele::IeleIntConstant::getOne(&Compiler->Context), SLoc, InsertAtEnd);
    iele::IeleInstruction::CreateLoad(OldValue, Spill, SLoc, InsertAtEnd);
    iele::IeleInstruction::CreateSStore(OldValue, Address, SLoc, InsertAtEnd);
  } else
    iele::IeleInstruction::CreateStore(Value->getValue(), Address, Offset, iele::IeleIntConstant::getOne(&Compiler->Context), SLoc, InsertAtEnd);
}

IeleRValue *ByteArrayLValue::read(const SourceLocation &SLoc, iele::IeleBlock *InsertAtEnd) const {
  iele::IeleLocalVariable *Result = iele::IeleLocalVariable::Create(&Compiler->Context , "string.val", Compiler->CompilingFunction);
  if (Loc == DataLocation::Storage) {
    iele::IeleInstruction::CreateSLoad(Result, Address, SLoc, InsertAtEnd);
    iele::IeleInstruction::CreateBinOp(iele::IeleInstruction::Byte, Result, Offset, Result, SLoc, InsertAtEnd);
  } else {
    iele::IeleInstruction::CreateLoad(Result, Address, Offset, iele::IeleIntConstant::getOne(&Compiler->Context), SLoc, InsertAtEnd);
    iele::IeleInstruction::CreateBinOp(iele::IeleInstruction::Byte, Result, iele::IeleIntConstant::getZero(&Compiler->Context), Result, SLoc, InsertAtEnd);
  }
  return IeleRValue::Create(Result);
}

void ArrayLengthLValue::write(IeleRValue *Value, const SourceLocation &SLoc, iele::IeleBlock *InsertAtEnd) const {
  solAssert(Loc == DataLocation::Storage, "invalid location for array length write");
  Compiler->appendArrayLengthResize(Address, Value->getValue());
  AddressLValue::write(Value, SLoc, Compiler->CompilingBlock);
}

RecursiveStructLValue::RecursiveStructLValue(
    IeleRValue *Value, const StructType &type, IeleCompiler *Compiler) :
  ReadOnlyLValue(Value), type(type), Compiler(Compiler) {}

IeleRValue *RecursiveStructLValue::read(const SourceLocation &SLoc, iele::IeleBlock *InsertAtEnd) const {
  solAssert(InsertAtEnd == Compiler->CompilingBlock,
            "IeleCompiler: requested code generation for recursive struct "
            "lvalue dereference in block other than the current compiling "
            "block");

  // Dereference the lvalue to get the address of the first slot of the struct.
  iele::IeleLocalVariable *RecStructAddress =
    iele::IeleLocalVariable::Create(
        &Compiler->Context, "rec.struct.addr", Compiler->CompilingFunction);
  iele::IeleInstruction::CreateSLoad(
      RecStructAddress, Value->getValue(), SLoc, InsertAtEnd);

  // Create join block.
  iele::IeleBlock *JoinBlock =
    iele::IeleBlock::Create(&Compiler->Context, "rec.struct.found");

  // Check for zero and jump to join block if pointer is not zero.
  Compiler->connectWithConditionalJump(
      RecStructAddress, InsertAtEnd, JoinBlock);

  // Allocate a new recursive struct and store the address of its first slot
  // in the lvalue.
  iele::IeleLocalVariable *RecStructAllocAddress =
    Compiler->appendRecursiveStructAllocation(type);
  iele::IeleInstruction::CreateSStore(
      RecStructAllocAddress, Value->getValue(), SLoc, InsertAtEnd);
  iele::IeleInstruction::CreateAssign(
      RecStructAddress, RecStructAllocAddress, SLoc, InsertAtEnd);

  // Continue code generation in the join block.
  JoinBlock->insertInto(Compiler->CompilingFunction);
  Compiler->CompilingBlock = JoinBlock;
  
  // Return the address of the first slot of the 
  // recusrive struct.
  return IeleRValue::Create(RecStructAddress);
}
