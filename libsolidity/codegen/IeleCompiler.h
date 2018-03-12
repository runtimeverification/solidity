#pragma once

#include "libiele/IeleContext.h"
#include "libiele/IeleContract.h"
#include "libsolidity/ast/ASTVisitor.h"
#include "libevmasm/LinkerObject.h"

#include "llvm/Support/raw_ostream.h"

namespace dev {
namespace iele {

class IeleContract;

} // end namespace iele

namespace solidity {

class IeleCompiler : public ASTConstVisitor {
public:
  explicit IeleCompiler() :
    CompiledContract(nullptr), CompilingContract(nullptr),
    CompilingFunction(nullptr), CompilingBlock(nullptr), BreakBlock(nullptr),
    ContinueBlock(nullptr), RevertBlock(nullptr), AssertFailBlock(nullptr),
    CompilingLValue(false) { }

  // Compiles a contract.
  void compileContract(
      ContractDefinition const &contract,
      std::map<ContractDefinition const*, iele::IeleContract const*> const &contracts);

  // Returns the compiled IeleContract.
  iele::IeleContract const& assembly() const { return *CompiledContract; }

  std::string assemblyString(StringMap const& _sourceCodes = StringMap()) const {
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
};

} // end namespace solidity
} // end namespace dev
