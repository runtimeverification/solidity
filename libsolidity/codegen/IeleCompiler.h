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
    CompilingBlock(nullptr),
    BreakBlock(nullptr),
    ContinueBlock(nullptr),
    RevertBlock(nullptr),
    AssertFailBlock(nullptr),
    CompilingLValue(false),
    CompilingLValueKind(LValueKind::Reg),
    CompilingContractASTNode(nullptr),
    CompilingFunctionASTNode(nullptr),
    ModifierDepth(-1) { }

  // Compiles a contract.
  void compileContract(
      const ContractDefinition &contract,
      const std::map<const ContractDefinition *, const iele::IeleContract *> &contracts);

  // Returns the compiled IeleContract.
  const iele::IeleContract &assembly() const { return *CompiledContract; }

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

  bool isMostDerived(const FunctionDefinition *d) const;
  bool isMostDerived(const VariableDeclaration *d) const;
  const ContractDefinition *contractFor(const Declaration *d) const;

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
  const iele::IeleContract *CompiledContract;
  iele::IeleContext Context;
  iele::IeleContract *CompilingContract;
  iele::IeleFunction *CompilingFunction;
  iele::IeleBlock *CompilingBlock;
  iele::IeleBlock *BreakBlock;
  iele::IeleBlock *ContinueBlock;
  iele::IeleBlock *RevertBlock;
  iele::IeleBlock *AssertFailBlock;
  llvm::SmallVector<iele::IeleValue *, 4> CompilingExpressionResult;
  bool CompilingLValue;

  enum LValueKind { Reg, Memory, Storage };
  LValueKind CompilingLValueKind;

  std::vector<const ContractDefinition *> CompilingContractInheritanceHierarchy;
  const ContractDefinition *CompilingContractASTNode;
  const FunctionDefinition *CompilingFunctionASTNode;

  std::string getIeleNameForFunction(const FunctionDefinition &function);
  std::string getIeleNameForStateVariable(const VariableDeclaration *stateVariable);

  // Infrastructure for handling modifiers (borrowed from ContractCompiler.cpp)
  // Lookup function modifier by name
  const ModifierDefinition &functionModifier(const std::string &_name) const;
  // Appends one layer of function modifier code of the current function, or the
  // function body itself if the last modifier was reached.
  void appendModifierOrFunctionCode();
  unsigned ModifierDepth;

  // Helpers for the compilation process.
  iele::IeleValue *compileExpression(const Expression &expression);
  void compileTuple(
      const Expression &expression,
      llvm::SmallVectorImpl<iele::IeleValue *> &Result);
  iele::IeleValue *compileLValue(const Expression &expression);

  void connectWithUnconditionalJump(iele::IeleBlock *SourceBlock, 
                                    iele::IeleBlock *DestinationBlock);
  void connectWithConditionalJump(iele::IeleValue *Condition,
                                  iele::IeleBlock *SouceBlock, 
                                  iele::IeleBlock *DestinationBlock);

  void appendRevert(iele::IeleValue *Condition = nullptr);
  void appendInvalid(iele::IeleValue *Condition = nullptr);

  // Infrastructure for unique variable names generation and mapping
  int NextUniqueIntToken = 0;
  int getNextUniqueIntToken();
  std::string getNextVarSuffix();
  // It is not enough to map names to names; we need such a mapping for each "layer",
  // that is for each function modifier. 
  std::map<unsigned, std::map <std::string, std::string>> VarNameMap;

  void appendStateVariableInitialization(const ContractDefinition *contract);

  iele::IeleLocalVariable *appendIeleRuntimeAllocateMemory(
      iele::IeleValue *NumElems);
  iele::IeleLocalVariable *appendIeleRuntimeAllocateStorage(
      iele::IeleValue *NumElems);
  iele::IeleLocalVariable *appendIeleRuntimeMemoryAddress(iele::IeleValue *Base,
                                                  iele::IeleValue *Offset);
  iele::IeleLocalVariable *appendIeleRuntimeStorageAddress(
      iele::IeleValue *Base, iele::IeleValue *Offset);
  iele::IeleLocalVariable *appendIeleRuntimeMemoryLoad(iele::IeleValue *Base,
                                                       iele::IeleValue *Offset);
  iele::IeleLocalVariable *appendIeleRuntimeStorageLoad(
      iele::IeleValue *Base, iele::IeleValue *Offset);
  iele::IeleLocalVariable *appendIeleRuntimeStorageToStorageCopy(
      iele::IeleValue *From);
  iele::IeleLocalVariable *appendIeleRuntimeMemoryToStorageCopy(
      iele::IeleValue *From);
  iele::IeleLocalVariable *appendIeleRuntimeStorageToMemoryCopy(
      iele::IeleValue *From);

  iele::IeleLocalVariable *appendLValueDereference(iele::IeleValue *LValue);
  void appendLValueAssign(iele::IeleValue *LValue, iele::IeleValue *RValue);

  iele::IeleLocalVariable *appendBinaryOperator(
      Token::Value Opcode, iele::IeleValue *LeftOperand,
      iele::IeleValue *RightOperand);

  iele::IeleLocalVariable *appendBooleanOperator(
      Token::Value Opcode,
      const Expression &LeftOperand,
      const Expression &RightOperand);

  iele::IeleLocalVariable *appendConditional(
      iele::IeleValue *ConditionValue,
      const std::function<iele::IeleValue *(void)> &TrueValue,
      const std::function<iele::IeleValue *(void)> &FalseValue);

  bool shouldCopyStorageToStorage(iele::IeleValue *To, TypePointer From) const;
  bool shouldCopyMemoryToStorage(TypePointer To, TypePointer From) const;
  bool shouldCopyStorageToMemory(TypePointer To, TypePointer From) const;

  unsigned getStructMemberIndex(const StructType &type,
                                const std::string &member) const;
};

} // end namespace solidity
} // end namespace dev
