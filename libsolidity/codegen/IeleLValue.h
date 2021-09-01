#pragma once
#include "libsolidity/codegen/IeleCompiler.h"

namespace solidity {
namespace frontend {

class IeleLValue
{
protected:
  explicit IeleLValue() {}

public:
  virtual void write(IeleRValue *Value, const langutil::SourceLocation &SLoc, iele::IeleBlock *InsertAtEnd) const = 0;
  virtual IeleRValue *read(const langutil::SourceLocation &SLoc, iele::IeleBlock *InsertAtEnd) const = 0;
};

class RegisterLValue : public IeleLValue {
private:
  RegisterLValue(std::vector<iele::IeleLocalVariable *> Var);

  std::vector<iele::IeleLocalVariable *> Var;
public:
  RegisterLValue(const RegisterLValue&) = delete;
  void operator=(const RegisterLValue&) = delete;

  ~RegisterLValue() = default;

  static RegisterLValue *Create(std::vector<iele::IeleLocalVariable *> Var) {
    return new RegisterLValue(Var);
  }

  virtual void write(IeleRValue *Value, const langutil::SourceLocation &SLoc, iele::IeleBlock *InsertAtEnd) const override;
  virtual IeleRValue *read(const langutil::SourceLocation &SLoc, iele::IeleBlock *InsertAtEnd) const override;
};

// used to represent array elements and struct members that are not dynamically allocated; ie, a dereference is not required in order to get an rvalue of reference type.
class ReadOnlyLValue : public IeleLValue {
protected:
  ReadOnlyLValue(IeleRValue *Value) : Value(Value) {}

  IeleRValue *Value;
public:
  ReadOnlyLValue(const ReadOnlyLValue&) = delete;
  void operator=(const ReadOnlyLValue&) = delete;

  ~ReadOnlyLValue() = default;

  static ReadOnlyLValue *Create(IeleRValue *Value) {
    return new ReadOnlyLValue(Value);
  }

  virtual void write(IeleRValue *Value, const langutil::SourceLocation &SLoc, iele::IeleBlock *InsertAtEnd) const override { solAssert(false, "Cannot write to ReadOnlyLValue"); }
  virtual IeleRValue *read(const langutil::SourceLocation &SLoc, iele::IeleBlock *InsertAtEnd) const override { return Value; }
};


class AddressLValue : public IeleLValue
{
protected:
  AddressLValue(IeleCompiler *Compiler, iele::IeleValue *Address, DataLocation Loc, std::string Name, unsigned Num);

  IeleCompiler *Compiler;
  iele::IeleValue *Address;
  DataLocation Loc;
  std::string Name;
  unsigned Num;
public:
  AddressLValue(const AddressLValue&) = delete;
  void operator=(const AddressLValue&) = delete;

  ~AddressLValue() = default;

  static AddressLValue *Create(IeleCompiler *Compiler, iele::IeleValue *Address, DataLocation Loc, std::string Name = "loaded", unsigned Num = 1) {
    return new AddressLValue(Compiler, Address, Loc, Name, Num);
  }

  virtual void write(IeleRValue *Value, const langutil::SourceLocation &SLoc, iele::IeleBlock *InsertAtEnd) const override;
  virtual IeleRValue *read(const langutil::SourceLocation &SLoc, iele::IeleBlock *InsertAtEnd) const override;
};

class ByteArrayLValue : public IeleLValue
{
private:
  ByteArrayLValue(IeleCompiler *Compiler, iele::IeleValue *Address, iele::IeleValue *Offset, DataLocation Loc);

  IeleCompiler *Compiler;
  iele::IeleValue *Address;
  iele::IeleValue *Offset;
  DataLocation Loc;
public:
  ByteArrayLValue(const ByteArrayLValue&) = delete;
  void operator=(const ByteArrayLValue&) = delete;

  ~ByteArrayLValue() = default;

  static ByteArrayLValue *Create(IeleCompiler *Compiler, iele::IeleValue *Address, iele::IeleValue *Offset, DataLocation Loc) {
    return new ByteArrayLValue(Compiler, Address, Offset, Loc);
  }

  virtual void write(IeleRValue *Value, const langutil::SourceLocation &SLoc, iele::IeleBlock *InsertAtEnd) const override;
  virtual IeleRValue *read(const langutil::SourceLocation &SLoc, iele::IeleBlock *InsertAtEnd) const override;
};

class ArrayLengthLValue : public AddressLValue
{
private:
  ArrayLengthLValue(IeleCompiler *Compiler, iele::IeleValue *Address, DataLocation Loc) : AddressLValue(Compiler, Address, Loc, "array.length", 1) {}

public:
  ArrayLengthLValue(const ArrayLengthLValue&) = delete;
  void operator=(const ArrayLengthLValue&) = delete;

  ~ArrayLengthLValue() = default;

  static ArrayLengthLValue *Create(IeleCompiler *Compiler, iele::IeleValue *Address, DataLocation Loc) {
    return new ArrayLengthLValue(Compiler, Address, Loc);
  }

  virtual void write(IeleRValue *Value, const langutil::SourceLocation &SLoc, iele::IeleBlock *InsertAtEnd) const override;
};

// A recursive struct lvalue should only be used to hold a reference to a
// recusrsive struct in storage. It has the same behavior as a readonly lvalue:
// the recursive struct referenced by it is to be treated as statically
// allocated and this lvalue cannot be written (one needs to use explicit
// copying APIs to generate code that overwrites the contents of the object
// referenced by this lvalue).
// However, it is implemented in a lazy fashion: it actually holds a
// pointer to the address of the first slot of the struct and this pointer is
// initially 0. When we attempt to read this lvalue, if the pointer is 0, a
// storage allocation happens and the pointer is set to point to the first slot
// of that allocation. Then, and in the case the pointer is not 0, we
// dereference the pointer to get the actual rvalue.
class RecursiveStructLValue : public ReadOnlyLValue
{
private:
  RecursiveStructLValue(
      IeleRValue *Value, const StructType &type, IeleCompiler *Compiler);

  const StructType &type;
  IeleCompiler *Compiler;

public:
  RecursiveStructLValue(const RecursiveStructLValue&) = delete;
  void operator=(const RecursiveStructLValue&) = delete;

  ~RecursiveStructLValue() = default;

  static RecursiveStructLValue *Create(
      IeleRValue *Value, const StructType &type, IeleCompiler *Compiler) {
    return new RecursiveStructLValue(Value, type, Compiler);
  }

  virtual IeleRValue *read(const langutil::SourceLocation &SLoc, iele::IeleBlock *InsertAtEnd) const override;
};

} // end namespace frontend
} // end namespace solidity
