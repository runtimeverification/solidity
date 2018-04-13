#pragma once

#include "libiele/IeleContext.h"
#include "libiele/IeleContract.h"
#include "libsolidity/ast/ASTVisitor.h"
#include "libevmasm/LinkerObject.h"

#include "llvm/Support/raw_ostream.h"

#include <map>

namespace dev {
namespace iele {

class IeleContract;

} // end namespace iele

namespace solidity {

class IeleCompiler : public ASTConstVisitor {
public:
  explicit IeleCompiler() :
    CompiledContract(nullptr),
    CompilingContract(nullptr),
    CompilingFunction(nullptr),
    CompilingFunctionStatus(nullptr),
    CompilingBlock(nullptr),
    BreakBlock(nullptr),
    ContinueBlock(nullptr),
    RevertBlock(nullptr),
    RevertStatusBlock(nullptr),
    AssertFailBlock(nullptr),
    CompilingLValue(false),
    CompilingLValueKind(LValueKind::Reg),
    GasValue(nullptr),
    TransferValue(nullptr),
    NextStorageAddress(0),
    CompilingContractASTNode(nullptr),
    CompilingFunctionASTNode(nullptr),
    ModifierDepth(-1) { }

  // Compiles a contract.
  void compileContract(
      const ContractDefinition &contract,
      const std::map<const ContractDefinition *, iele::IeleContract *> &contracts);

  // Returns the compiled IeleContract.
  iele::IeleContract &assembly() const { return *CompiledContract; }

  std::string assemblyString(const StringMap &_sourceCodes = StringMap()) const {
    std::string ret;
    llvm::raw_string_ostream OS(ret);
    CompiledContract->print(OS);
    return ret;
  }

  eth::LinkerObject assembledObject() const {
    bytes bytecode = CompiledContract->toBinary();
    return {bytecode, std::map<size_t, std::string>()};
  }

  // Visitor interface.
  virtual bool visit(const FunctionDefinition &function) override;
  virtual bool visit(const IfStatement &ifStatement) override;
  virtual bool visit(const WhileStatement &whileStatement) override;
  virtual bool visit(const ForStatement &forStatement) override;
  virtual bool visit(const Continue &continueStatement) override;
  virtual bool visit(const Break &breakStatement) override;
  virtual bool visit(const Return &returnStatement) override;
  virtual bool visit(const Throw &throwStatement) override;
  virtual bool visit(
      const VariableDeclarationStatement &variableDeclarationStatement)
      override;
  virtual bool visit(const ExpressionStatement &expressionStatement) override;
  virtual bool visit(const PlaceholderStatement &placeholderStatement)
     override;
  virtual bool visit(const InlineAssembly &inlineAssembly) override;

  virtual bool visit(const Conditional &condition) override;
  virtual bool visit(const Assignment &assignment) override;
  virtual bool visit(const TupleExpression &tuple) override;
  virtual bool visit(const UnaryOperation &unaryOperation) override;
  virtual bool visit(const BinaryOperation &binaryOperation) override;
  virtual bool visit(const FunctionCall &functionCall) override;
  virtual bool visit(const NewExpression &newExpression) override;
  virtual bool visit(const MemberAccess &memberAccess) override;
  virtual bool visit(const IndexAccess &indexAccess) override;
  virtual void endVisit(const Identifier &identifier) override;
  virtual void endVisit(const Literal &literal) override;

private:
  iele::IeleContract *CompiledContract;
  std::map<const ContractDefinition *, iele::IeleContract *> CompiledContracts;
  iele::IeleContext Context;
  iele::IeleContract *CompilingContract;
  iele::IeleFunction *CompilingFunction;
  iele::IeleLocalVariable *CompilingFunctionStatus;
  iele::IeleBlock *CompilingBlock;
  iele::IeleBlock *BreakBlock;
  iele::IeleBlock *ContinueBlock;
  iele::IeleBlock *RevertBlock;
  iele::IeleBlock *RevertStatusBlock;
  iele::IeleBlock *AssertFailBlock;
  llvm::SmallVector<iele::IeleValue *, 4> CompilingExpressionResult;
  bool CompilingLValue;

  enum LValueKind { Reg, Memory, Storage, ArrayLengthMemory, ArrayLengthStorage };
  LValueKind CompilingLValueKind;
  bigint CompilingLValueArrayElementSize;

  iele::IeleValue *GasValue;
  iele::IeleValue *TransferValue;

  bigint NextStorageAddress;

  std::vector<const ContractDefinition *> CompilingContractInheritanceHierarchy;
  const ContractDefinition *CompilingContractASTNode;
  const FunctionDefinition *CompilingFunctionASTNode;

  std::string getIeleNameForContract(const ContractDefinition *function);
  std::string getIeleNameForFunction(const FunctionDefinition &function);
  std::string getIeleNameForStateVariable(const VariableDeclaration *stateVariable);

  // Infrastructure for handling modifiers (borrowed from ContractCompiler.cpp)
  // Lookup function modifier by name
  const ModifierDefinition &functionModifier(const std::string &_name) const;
  // Appends one layer of function modifier code of the current function, or the
  // function body itself if the last modifier was reached.
  void appendModifierOrFunctionCode();
  unsigned ModifierDepth;

  template <class ArgClass, class ReturnClass, class ExpressionClass>
  void compileFunctionArguments(ArgClass *Arguments, ReturnClass *Returns, const::std::vector<ASTPointer<ExpressionClass>> &arguments, const FunctionType &function);

  // Infrastructure for unique variable names generation and mapping
  int NextUniqueIntToken = 0;
  int getNextUniqueIntToken();
  std::string getNextVarSuffix();
  // It is not enough to map names to names; we need such a mapping for each "layer",
  // that is for each function modifier.
  std::map<unsigned, std::map <std::string, std::string>> VarNameMap;

  // Helpers for the compilation process.
  iele::IeleValue *compileExpression(const Expression &expression);
  void compileExpressions(
      const Expression &Expression,
      llvm::SmallVectorImpl<iele::IeleValue *> &Result,
      unsigned numExpressions);
  void compileTuple(
      const Expression &expression,
      llvm::SmallVectorImpl<iele::IeleValue *> &Result);
  iele::IeleValue *compileLValue(const Expression &expression);
  void compileLValues(
      const Expression &expression,
      llvm::SmallVectorImpl<iele::IeleValue *> &Result);

  void connectWithUnconditionalJump(iele::IeleBlock *SourceBlock, 
                                    iele::IeleBlock *DestinationBlock);
  void connectWithConditionalJump(iele::IeleValue *Condition,
                                  iele::IeleBlock *SouceBlock, 
                                  iele::IeleBlock *DestinationBlock);

  void appendPayableCheck(void);
  void appendRevert(iele::IeleValue *Condition = nullptr, iele::IeleValue *Status = nullptr);
  void appendInvalid(iele::IeleValue *Condition = nullptr);
  void appendRangeCheck(iele::IeleValue *Value, bigint *min, bigint *max);
  void appendRangeCheck(iele::IeleValue *Value, const Type &Type);

  void appendRevertBlocks(void);

  void appendDefaultConstructor(const ContractDefinition *contract);
  void appendLocalVariableInitialization(
          iele::IeleLocalVariable *Local, const VariableDeclaration *localVariable);

  iele::IeleValue *getReferenceTypeSize(
    const Type &type, iele::IeleValue *AddressValue);

  // Helper function for memory array allocation. The optional NumElemsValue
  // argument can be used to allocate dynamically-sized arrays, where the initial
  // size is not known at compile time.
  iele::IeleLocalVariable *appendArrayAllocation(
      const ArrayType &type, iele::IeleValue *NumElemsValue = nullptr, bool StoreLength = true);

  // Helper function for memory struct allocation.
  iele::IeleLocalVariable *appendStructAllocation(const StructType &type);

  // Helper functions for copying a reference type to a storage location.
  void appendCopyFromStorageToStorage(
      iele::IeleValue *To, const Type &ToType, iele::IeleValue *From, const Type &FromType);
  void appendCopyFromMemoryToStorage(
      iele::IeleValue *To, const Type &ToType, iele::IeleValue *From, const Type &FromType);

  void appendCopyFromMemoryToMemory(
      iele::IeleValue *To, const Type &ToType, iele::IeleValue *From, const Type &FromType);

  // Helper function for copying a storage reference type into a local newly
  // allocated memory copy. Returns a pointer to the copy.
  iele::IeleValue *appendCopyFromStorageToMemory(
    const Type &ToType, iele::IeleValue *From, const Type &FromType);

  void appendCopy(
      iele::IeleValue *To, const Type &ToType, iele::IeleValue *From, const Type &FromType, DataLocation ToLoc, DataLocation FromLoc);

  void appendAccessorFunction(const VariableDeclaration *stateVariable);

  void appendVariable(iele::IeleValue *Identifier, std::string name,
                      bool isValueType = true);

  iele::IeleLocalVariable *appendLValueDereference(iele::IeleValue *LValue);
  void appendLValueAssign(iele::IeleValue *LValue, iele::IeleValue *RValue);
  void appendLValueDelete(iele::IeleValue *LValue, TypePointer Type);
  void appendArrayLengthResize(bool Storage, iele::IeleValue *LValue, iele::IeleValue *NewLength);

  iele::IeleLocalVariable *appendBinaryOperator(
      Token::Value Opcode, iele::IeleValue *LeftOperand,
      iele::IeleValue *RightOperand,
      TypePointer ResultType);

  void appendShiftBy(iele::IeleLocalVariable *Result, iele::IeleValue *Value,
      int shiftAmount);
  void appendMask(iele::IeleLocalVariable *Result, iele::IeleValue *Value, 
      int bytes, bool issigned);
  iele::IeleValue *appendTypeConversion(
      iele::IeleValue *Operand,
      const Type& SourceType,
      const Type& TargetType);

  void appendTypeConversions(
    llvm::SmallVectorImpl<iele::IeleValue *> &Results, 
    llvm::SmallVectorImpl<iele::IeleValue *> &RHSValues,
    TypePointer SourceType, TypePointers TargetTypes);

  iele::IeleLocalVariable *appendBooleanOperator(
      Token::Value Opcode,
      const Expression &LeftOperand,
      const Expression &RightOperand);

  void appendConditional(
      iele::IeleValue *ConditionValue,
      llvm::SmallVectorImpl<iele::IeleLocalVariable *> &Results,
      const std::function<void(llvm::SmallVectorImpl<iele::IeleValue *> &)> &TrueExpression,
      const std::function<void(llvm::SmallVectorImpl<iele::IeleValue *> &)> &FalseExpression);

  void appendConditionalBranch(
    llvm::SmallVectorImpl<iele::IeleValue *> &Results,
    const Expression &Branch,
    TypePointer Type);

  bool shouldCopyStorageToStorage(const iele::IeleValue *To, const Type &From) const;
  bool shouldCopyMemoryToStorage(const iele::IeleValue *To, const Type &From) const;
  bool shouldCopyStorageToMemory(const Type &To, const Type &From) const;

  bool isMostDerived(const FunctionDefinition *d) const;
  bool isMostDerived(const VariableDeclaration *d) const;
  const ContractDefinition *contractFor(const Declaration *d) const;
  const FunctionDefinition *superFunction(const FunctionDefinition &function, const ContractDefinition &contract);
  const FunctionDefinition *resolveVirtualFunction(const FunctionDefinition &function);
  const FunctionDefinition *resolveVirtualFunction(const FunctionDefinition &function, std::vector<const ContractDefinition *>::iterator it);

  // IELE Runtime functionality. These methods append calls to the IELE memory
  // management runtime and indicate that the runtime should be included as part
  // of the compiling contract.
  iele::IeleLocalVariable *appendIeleRuntimeAllocateMemory(
      iele::IeleValue *NumSlots);
  void appendIeleRuntimeFill(
      iele::IeleValue *To, iele::IeleValue *NumSlots, iele::IeleValue *Value,
      DataLocation Loc);
  void appendIeleRuntimeCopy(
      iele::IeleValue *From, iele::IeleValue *To, iele::IeleValue *NumSlots,
      DataLocation FromLoc, DataLocation ToLoc);
};

} // end namespace solidity
} // end namespace dev
