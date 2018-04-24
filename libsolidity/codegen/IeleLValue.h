#pragma once
#include "libsolidity/codegen/IeleCompiler.h"

namespace dev
{
namespace solidity
{

class IeleLValue
{
protected:
  explicit IeleLValue() {}

public:
  virtual void write(iele::IeleValue *Value, iele::IeleBlock *InsertAtEnd) const = 0;
  virtual iele::IeleValue *read(iele::IeleBlock *InsertAtEnd) const = 0;
};

class RegisterLValue : public IeleLValue {
private:
  RegisterLValue(iele::IeleLocalVariable *Var);

  iele::IeleLocalVariable *Var;
public:
  RegisterLValue(const RegisterLValue&) = delete;
  void operator=(const RegisterLValue&) = delete;

  ~RegisterLValue() = default;

  static RegisterLValue *Create(iele::IeleLocalVariable *Var) {
    return new RegisterLValue(Var);
  }

  virtual void write(iele::IeleValue *Value, iele::IeleBlock *InsertAtEnd) const override;
  virtual iele::IeleValue *read(iele::IeleBlock *InsertAtEnd) const override;
};

// used to represent array elements and struct members that are not dynamically allocated; ie, a dereference is not required in order to get an rvalue of reference type.
class ReadOnlyLValue : public IeleLValue {
private:
  ReadOnlyLValue(iele::IeleValue *Value) : Value(Value) {}

  iele::IeleValue *Value;
public:
  ReadOnlyLValue(const RegisterLValue&) = delete;
  void operator=(const ReadOnlyLValue&) = delete;

  ~ReadOnlyLValue() = default;

  static ReadOnlyLValue *Create(iele::IeleValue *Value) {
    return new ReadOnlyLValue(Value);
  }

  virtual void write(iele::IeleValue *Value, iele::IeleBlock *InsertAtEnd) const override { solAssert(false, "Cannot write to ReadOnlyLValue"); }
  virtual iele::IeleValue *read(iele::IeleBlock *InsertAtEnd) const override { return Value; }
};


class AddressLValue : public IeleLValue
{
protected:
  AddressLValue(IeleCompiler *Compiler, iele::IeleValue *Address, DataLocation Loc, std::string Name);

  IeleCompiler *Compiler;
  iele::IeleValue *Address;
  DataLocation Loc;
  std::string Name;
public:
  AddressLValue(const AddressLValue&) = delete;
  void operator=(const AddressLValue&) = delete;

  ~AddressLValue() = default;

  static AddressLValue *Create(IeleCompiler *Compiler, iele::IeleValue *Address, DataLocation Loc, std::string Name = "loaded") {
    return new AddressLValue(Compiler, Address, Loc, Name);
  }

  virtual void write(iele::IeleValue *Value, iele::IeleBlock *InsertAtEnd) const override;
  virtual iele::IeleValue *read(iele::IeleBlock *InsertAtEnd) const override;
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

  virtual void write(iele::IeleValue *Value, iele::IeleBlock *InsertAtEnd) const override;
  virtual iele::IeleValue *read(iele::IeleBlock *InsertAtEnd) const override;
};

class ArrayLengthLValue : public AddressLValue
{
private:
  ArrayLengthLValue(IeleCompiler *Compiler, iele::IeleValue *Address, DataLocation Loc) : AddressLValue(Compiler, Address, Loc, "array.length") {}

public:
  ArrayLengthLValue(const ArrayLengthLValue&) = delete;
  void operator=(const ArrayLengthLValue&) = delete;

  ~ArrayLengthLValue() = default;

  static ArrayLengthLValue *Create(IeleCompiler *Compiler, iele::IeleValue *Address, DataLocation Loc) {
    return new ArrayLengthLValue(Compiler, Address, Loc);
  }

  virtual void write(iele::IeleValue *Value, iele::IeleBlock *InsertAtEnd) const override;
};

}
}
