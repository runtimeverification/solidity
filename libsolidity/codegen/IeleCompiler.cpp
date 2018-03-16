#include "libsolidity/codegen/IeleCompiler.h"

#include <libsolidity/interface/Exceptions.h>

#include "libiele/IeleContract.h"
#include "libiele/IeleGlobalVariable.h"
#include "libiele/IeleIntConstant.h"

#include <iostream>
#include "llvm/Support/raw_ostream.h"

using namespace dev;
using namespace dev::solidity;

// lookup a ModifierDefinition by name (borrowed from CompilerContext.cpp)
const ModifierDefinition &IeleCompiler::functionModifier(
    const std::string &_name) const {
  //solAssert(!m_inheritanceHierarchy.empty(), "No inheritance hierarchy set.");
  //for (ContractDefinition const* contract: m_inheritanceHierarchy)
  solAssert(CompilingContractASTNode, "CurrentContract not set.");
  for (ModifierDefinition const* modifier: CompilingContractASTNode->functionModifiers())
    if (modifier->name() == _name)
      return *modifier;
  solAssert(false, "IeleCompiler: Function modifier not found.");
}

void IeleCompiler::compileContract(
    const ContractDefinition &contract,
    const std::map<const ContractDefinition *, const iele::IeleContract *> &contracts) {
  
  // Store the current contract
  CompilingContractASTNode = &contract;
  
  // Create IeleContract.
  CompilingContract = iele::IeleContract::Create(&Context, contract.name());
  CompilingContractASTNode = &contract;

  // Visit state variables.
  llvm::APInt NextStorageAddress(64, 1); // here we should use a true unbound
                                         // integer datatype but using llvm's
                                         // APINt for now.
  for (const VariableDeclaration *stateVariable : contract.stateVariables()) {
    iele::IeleGlobalVariable *GV =
      iele::IeleGlobalVariable::Create(&Context, stateVariable->name(),
                                       CompilingContract);
    solAssert(NextStorageAddress != 0,
           "IeleCompiler: Overflow: more state variables than currently "
           "supported");
    GV->setStorageAddress(iele::IeleIntConstant::Create(&Context,
                                                        NextStorageAddress++));
  }

  // Visit fallback.
  if (const FunctionDefinition *fallback = contract.fallbackFunction())
    fallback->accept(*this);

  // Visit constructor. If it doesn't exist create an empty @init function as
  // constructor.
  if (const FunctionDefinition *constructor = contract.constructor())
    constructor->accept(*this);
  else {
    CompilingFunction =
      iele::IeleFunction::Create(&Context, true, "init", CompilingContract);
    CompilingBlock =
      iele::IeleBlock::Create(&Context, "entry", CompilingFunction);
    appendStateVariableInitialization();
    iele::IeleInstruction::CreateRetVoid(CompilingBlock);
    CompilingBlock = nullptr;
    CompilingFunction = nullptr;
  }

  // Visit functions.
  for (const FunctionDefinition *function : contract.definedFunctions()) {
    if (function->isConstructor() || function->isFallback())
      continue;
    function->accept(*this);
  }

  // Store compilation result.
  CompiledContract = CompilingContract;
}

int IeleCompiler::getNextUniqueIntToken() {
  return NextUniqueIntToken++;
}

std::string IeleCompiler::getNextVarSuffix() {
  return ("_" + std::to_string(getNextUniqueIntToken()));
}

bool IeleCompiler::visit(const FunctionDefinition &function) {
  std::string FunctionName;
  if (function.isConstructor())
    FunctionName = "init";
  else if (function.isFallback())
    FunctionName = "deposit";
  else if (function.isPublic())
    FunctionName = function.externalSignature();
  else
    // TODO: overloading on internal functions.
    FunctionName = function.name();

  CompilingFunction =
    iele::IeleFunction::Create(&Context, function.isPartOfExternalInterface(),
                               FunctionName, CompilingContract);
  CompilingFunctionASTNode = &function;
  unsigned NumOfModifiers = CompilingFunctionASTNode->modifiers().size();

  // Visit formal arguments and return parameters.
  for (const ASTPointer<const VariableDeclaration> &arg : function.parameters()) {
    std::string genName = arg->name() + getNextVarSuffix();
    VarNameMap[NumOfModifiers][arg->name()] = genName; 
    iele::IeleArgument::Create(&Context, genName, CompilingFunction);
  }

  for (const ASTPointer<const VariableDeclaration> &ret : function.returnParameters()) {
    std::string genName = ret->name() + getNextVarSuffix();
    VarNameMap[NumOfModifiers][ret->name()] = genName; 
    iele::IeleLocalVariable::Create(&Context, genName, CompilingFunction);
  }

  // Visit local variables.
  for (const VariableDeclaration *local: function.localVariables()) {
    std::string genName = local->name() + getNextVarSuffix();
    VarNameMap[NumOfModifiers][local->name()] = genName; 
    iele::IeleLocalVariable::Create(&Context, genName, CompilingFunction);
  }

  // Create the entry block.
  CompilingBlock =
    iele::IeleBlock::Create(&Context, "entry", CompilingFunction);

  // If the function is a constructor, visit state variables and add
  // initialization code.
  if (function.isConstructor())
    appendStateVariableInitialization();

  // Visit function body (inc modifiers). 
  CompilingFunctionASTNode = &function;
  ModifierDepth = -1;
  appendModifierOrFunctionCode();

  // Add a ret void if the last block doesn't end with a ret instruction.
  if (!CompilingBlock->endsWithRet())
    iele::IeleInstruction::CreateRetVoid(CompilingBlock);

  // Append the revert block if needed.
  if (RevertBlock) {
    RevertBlock->insertInto(CompilingFunction);
    RevertBlock = nullptr;
  }

  // Append the assert fail block if needed.
  if (AssertFailBlock) {
    AssertFailBlock->insertInto(CompilingFunction);
    AssertFailBlock = nullptr;
  }

  CompilingBlock = nullptr;
  CompilingFunction = nullptr;
  return false;
}

bool IeleCompiler::visit(const IfStatement &ifStatement) {
  // Check if we have an if-false block. Our compilation strategy depends on
  // that.
  bool HasIfFalse = ifStatement.falseStatement() != nullptr;

  // Visit condition.
  iele::IeleValue * ConditionValue =
    compileExpression(ifStatement.condition());
  solAssert(ConditionValue, "IeleCompiler: Failed to compile if condition.");

  // If we don't have an if-false block, then we invert the condition.
  if (!HasIfFalse) {
    iele::IeleLocalVariable *InvConditionValue =
      iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
    iele::IeleInstruction::CreateIsZero(InvConditionValue, ConditionValue,
                                        CompilingBlock);
    ConditionValue = InvConditionValue;
  }

  // If we have an if-false block, then the condition target block is the if-true
  // block, else it is the if-end block.
  iele::IeleBlock *CondTargetBlock =
    iele::IeleBlock::Create(&Context, HasIfFalse ? "if.true" : "if.end");

  // Connect the condition block with a conditional jump to the condition target
  // block.
  connectWithConditionalJump(ConditionValue, CompilingBlock, CondTargetBlock);

  // Append the code for the if-false block if we have one, else append the code
  // for the if-true block. The code is appended to the condition block.
  if (HasIfFalse)
    ifStatement.falseStatement()->accept(*this);
  else
    ifStatement.trueStatement().accept(*this);

  // If we have an if-false block, then we need a new join block to jump to,
  // else the join block is the condition target block.
  iele::IeleBlock *JoinBlock = CondTargetBlock;
  if (HasIfFalse) {
    iele::IeleBlock *IfTrueBlock = CondTargetBlock;
    JoinBlock = iele::IeleBlock::Create(&Context, "if.end");

    connectWithUnconditionalJump(CompilingBlock, JoinBlock);

    // Add the if-true block at the end of the function and generate its code.
    IfTrueBlock->insertInto(CompilingFunction);
    CompilingBlock = IfTrueBlock;
    ifStatement.trueStatement().accept(*this);
  }

  // Add the join block at the end of the function and compilation continues in
  // the join block.
  JoinBlock->insertInto(CompilingFunction);
  CompilingBlock = JoinBlock;
  return false;
}

bool IeleCompiler::visit(const Return &returnStatement) {
  const Expression *returnExpr = returnStatement.expression();

  if (!returnExpr) {
    // Create ret void.
    iele::IeleInstruction::CreateRetVoid(CompilingBlock);
    return false;
  }

  // Visit return expression.
  llvm::SmallVector<iele::IeleValue*, 4> ReturnValues;
  compileTuple(*returnExpr, ReturnValues);
  iele::IeleInstruction::CreateRet(ReturnValues, CompilingBlock);

  return false;
}

void IeleCompiler::appendModifierOrFunctionCode() {
  solAssert(CompilingFunctionASTNode,
         "IeleCompiler: CompilingFunctionASTNode not defined");
  
  Block const* codeBlock = nullptr;

  ModifierDepth++;

  // The function we are processing has no modifiers. 
  // Process function body as normal...
  if (ModifierDepth >= CompilingFunctionASTNode->modifiers().size()) {
    solAssert(CompilingFunctionASTNode->isImplemented(),
              "IeleCompiler: Current function is not implemented");
    codeBlock = &CompilingFunctionASTNode->body();
  }
  // The function we are processing uses modifiers.
  else {
    // Get next modifier invocation
    ASTPointer<ModifierInvocation> const& modifierInvocation =
      CompilingFunctionASTNode->modifiers()[ModifierDepth];

    // constructor call should be excluded (and managed separeately)
    if (dynamic_cast<ContractDefinition const*>(modifierInvocation->name()->annotation().referencedDeclaration)) {
      solAssert(false, "IeleCompiler: function modifiers on constructor not implemented yet.");
      appendModifierOrFunctionCode();
    }
    else {
      // Retrieve modifier definition from its name
      ModifierDefinition const& modifier = functionModifier(modifierInvocation->name()->name());

      // Visit the modifier's parameters
      for (const ASTPointer<const VariableDeclaration> &arg : modifier.parameters()) {
        std::string genName = arg->name() + getNextVarSuffix();
        VarNameMap[ModifierDepth][arg->name()] = genName;
        iele::IeleLocalVariable::Create(&Context, genName, CompilingFunction);
      }

      // Visit the modifier's local variables
      for (const VariableDeclaration *local: modifier.localVariables()) {
        std::string genName = local->name() + getNextVarSuffix();
        VarNameMap[ModifierDepth][local->name()] = genName;
        iele::IeleLocalVariable::Create(&Context, genName, CompilingFunction);
      }

      // Is the modifier invocation well formed?
      solAssert(modifier.parameters().size() == modifierInvocation->arguments().size(),
             "IeleCompiler: modifier has wrong number of parameters!");

      // Get Symbol Table
      iele::IeleValueSymbolTable *ST = CompilingFunction->getIeleValueSymbolTable();
      solAssert(ST,
            "IeleCompiler: failed to access compiling function's symbol "
            "table while processing function modifer. ");

      // Cycle through each parameter-argument pair; for each one, make an assignment.
      // This way, we pass arguments into the modifier.
      for (unsigned i = 0; i < modifier.parameters().size(); ++i) {
        // Extract LHS and RHS from modifier definition and invocation
        VariableDeclaration const& var = *modifier.parameters()[i];
        Expression const& initValue    = *modifierInvocation->arguments()[i];

        // Temporarily set ModiferDepth to the level where all "top-level" (i.e. non-modifer related)
        // variable names are found; then, evaluate the RHS in this context;
        unsigned ModifierDepthCache = ModifierDepth;
        ModifierDepth = CompilingFunctionASTNode->modifiers().size();
        // Compile RHS expression
        iele::IeleValue* RHSValue = compileExpression(initValue);
        // Restore ModiferDepth to its original value
        ModifierDepth = ModifierDepthCache;

        // Lookup LHS from symbol table
        iele::IeleValue *LHSValue = ST->lookup(VarNameMap[ModifierDepth][var.name()]);
        solAssert(LHSValue, "IeleCompiler: Failed to compile argument to modifier invocation");

        // Make assignment
        iele::IeleInstruction::CreateAssign(
            llvm::cast<iele::IeleLocalVariable>(LHSValue), RHSValue, CompilingBlock);
      }

      // Arguments to the modifier have been taken care off. Now move to modifier's body.
      codeBlock = &modifier.body();
    }
  }

  // Visit whatever is next (modifier's body or function body)
  if (codeBlock) {
    codeBlock->accept(*this);
  }

  ModifierDepth--;
}

bool IeleCompiler::visit(const Throw &throwStatement) {
  appendRevert();
  return false;
}

bool IeleCompiler::visit(const WhileStatement &whileStatement) {
  // Create the loop body block.
  iele::IeleBlock *LoopBodyBlock =
    iele::IeleBlock::Create(&Context, "while.loop", CompilingFunction);
  CompilingBlock = LoopBodyBlock;

  // Create the loop condition block, in case of a do-while loop.
  iele::IeleBlock *LoopCondBlock = nullptr;
  if (whileStatement.isDoWhile())
    LoopCondBlock =
      iele::IeleBlock::Create(&Context, "while.cond");

  // Create the loop exit block.
  iele::IeleBlock *LoopExitBlock =
    iele::IeleBlock::Create(&Context, "while.end");

  if (!whileStatement.isDoWhile()) {
    // In a while loop, we first visit the condition.
    iele::IeleValue * ConditionValue =
      compileExpression(whileStatement.condition());
    solAssert(ConditionValue,
           "IeleCompiler: Failed to compile while condition.");

    // Invert the condition.
    iele::IeleLocalVariable *InvConditionValue =
      iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
    iele::IeleInstruction::CreateIsZero(InvConditionValue, ConditionValue,
                                        CompilingBlock);
    ConditionValue = InvConditionValue;

    // Branch out of the loop if the condition doesn't hold.
    connectWithConditionalJump(ConditionValue, CompilingBlock, LoopExitBlock);
  }

  // Visit loop body.
  iele::IeleBlock *CurrentBreakBlock = BreakBlock;
  iele::IeleBlock *CurrentContinueBlock = ContinueBlock;
  BreakBlock = LoopExitBlock;
  ContinueBlock = whileStatement.isDoWhile() ? LoopCondBlock : LoopBodyBlock;
  whileStatement.body().accept(*this);
  BreakBlock = CurrentBreakBlock;
  ContinueBlock = CurrentContinueBlock;

  if (whileStatement.isDoWhile()) {
    // In a do-while loop, we visit the condition after the loop body.
    LoopCondBlock->insertInto(CompilingFunction);
    CompilingBlock = LoopCondBlock;
    iele::IeleValue * ConditionValue =
      compileExpression(whileStatement.condition());
    solAssert(ConditionValue,
           "IeleCompiler: Failed to compile do-while condition.");

    // Branch to the start of the loop if the condition holds.
    connectWithConditionalJump(ConditionValue, CompilingBlock, LoopBodyBlock);
  } else {
    // In a while loop, we jump to the start of the loop unconditionally.
    connectWithUnconditionalJump(CompilingBlock, LoopBodyBlock);
  }

  // Compilation continues in the loop exit block.
  LoopExitBlock->insertInto(CompilingFunction);
  CompilingBlock = LoopExitBlock;
  return false;
}

bool IeleCompiler::visit(const ForStatement &forStatement) {
  // Visit initialization code if it exists.
  if (forStatement.initializationExpression())
    forStatement.initializationExpression()->accept(*this);

  // Create the loop body block.
  iele::IeleBlock *LoopBodyBlock =
    iele::IeleBlock::Create(&Context, "for.loop", CompilingFunction);
  CompilingBlock = LoopBodyBlock;

  // Create the loop increment block, if needed.
  iele::IeleBlock *LoopIncBlock = nullptr;
  if (forStatement.loopExpression())
    LoopIncBlock = iele::IeleBlock::Create(&Context, "for.inc");

  // Create the loop exit block.
  iele::IeleBlock *LoopExitBlock = iele::IeleBlock::Create(&Context, "for.end");

  // Visit condition, if it exists.
  if (forStatement.condition()) {
    iele::IeleValue * ConditionValue =
      compileExpression(*forStatement.condition());
    solAssert(ConditionValue, "IeleCompiler: Failed to compile for condition.");

    // Invert the condition.
    iele::IeleLocalVariable *InvConditionValue =
      iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
    iele::IeleInstruction::CreateIsZero(InvConditionValue, ConditionValue,
                                        CompilingBlock);
    ConditionValue = InvConditionValue;

    // Branch out of the loop if the condition doesn't hold.
    connectWithConditionalJump(ConditionValue, CompilingBlock, LoopExitBlock);
  }

  // Visit loop body.
  // TODO: Not sure how to handle break and especially continue appearing in the
  // loop header (i.e. init statement and/or loop increment). Is it even
  // possible?
  iele::IeleBlock *CurrentBreakBlock = BreakBlock;
  iele::IeleBlock *CurrentContinueBlock = ContinueBlock;
  BreakBlock = LoopExitBlock;
  ContinueBlock = forStatement.loopExpression() ? LoopIncBlock : LoopBodyBlock;
  forStatement.body().accept(*this);
  BreakBlock = CurrentBreakBlock;
  ContinueBlock = CurrentContinueBlock;

  if (forStatement.loopExpression()) {
    // Visit the loop increment code.
    LoopIncBlock->insertInto(CompilingFunction);
    CompilingBlock = LoopIncBlock;
    forStatement.loopExpression()->accept(*this);
  }

  // Jump back to the start of the loop.
  connectWithUnconditionalJump(CompilingBlock, LoopBodyBlock);

  // Compilation continues in the loop exit block.
  LoopExitBlock->insertInto(CompilingFunction);
  CompilingBlock = LoopExitBlock;
  return false;
}

bool IeleCompiler::visit(const Continue &continueStatement) {
  connectWithUnconditionalJump(CompilingBlock, ContinueBlock);
  return false;
}

bool IeleCompiler::visit(const Break &breakStatement) {
  connectWithUnconditionalJump(CompilingBlock, BreakBlock);
  return false;
}

bool IeleCompiler::visit(
    const VariableDeclarationStatement &variableDeclarationStatement) {
  if (const Expression* rhsExpr =
        variableDeclarationStatement.initialValue()) {

    // Visit RHS expression.
    llvm::SmallVector<iele::IeleValue*, 4> RHSValues;
    compileTuple(*rhsExpr, RHSValues);

    // Get RHS types
    TypePointers RHSTypes;
    if (const TupleType *tupleType =
          dynamic_cast<const TupleType *>(rhsExpr->annotation().type.get()))
      RHSTypes = tupleType->components();
    else
      RHSTypes = TypePointers{rhsExpr->annotation().type};

    // Visit assignments.
    auto const &assignments =
      variableDeclarationStatement.annotation().assignments;
    solAssert(assignments.size() == RHSValues.size(),
           "IeleCompiler: Missing assignment in variable declaration "
           "statement");
    iele::IeleValueSymbolTable *ST =
      CompilingFunction->getIeleValueSymbolTable();
    solAssert(ST,
          "IeleCompiler: failed to access compiling function's symbol "
          "table.");
    for (unsigned i = 0; i < assignments.size(); ++i) {
      const VariableDeclaration *varDecl = assignments[i];
      if (varDecl) {
        solAssert(varDecl->type()->category() != Type::Category::Function,
                  "not implemented yet");
        // Visit LHS. We lookup the LHS name in the function's symbol table,
        // where we should find it.
        iele::IeleValue *LHSValue =
          ST->lookup(VarNameMap[ModifierDepth][varDecl->name()]);
        solAssert(LHSValue, "IeleCompiler: Failed to compile LHS of variable "
                           "declaration statement");
        // Check if we need to do a memory to storage copy.
        TypePointer LHSType = varDecl->annotation().type;
        TypePointer RHSType = RHSTypes[i];
        iele::IeleValue *RHSValue = RHSValues[i];
        solAssert(!shouldCopyStorageToStorage(LHSValue, RHSType),
                  "IeleCompiler: found copy storage to storage in a variable "
                  "declaration statement");
        solAssert(!shouldCopyMemoryToStorage(LHSType, RHSType),
                  "IeleCompiler: found copy memory to storage in a variable "
                  "declaration statement");
        if (shouldCopyStorageToMemory(LHSType, RHSType))
          RHSValue = appendIeleRuntimeStorageToMemoryCopy(RHSValue);

        // Assign to RHS.
        iele::IeleInstruction::CreateAssign(
            llvm::cast<iele::IeleLocalVariable>(LHSValue), RHSValue,
            CompilingBlock);
      }
    }
  }
  return false;
}

bool IeleCompiler::visit(const ExpressionStatement &expressionStatement) {
  compileExpression(expressionStatement.expression());
  return false;
}

bool IeleCompiler::visit(const PlaceholderStatement &placeholderStatement) {
  appendModifierOrFunctionCode();
  return false;
}

bool IeleCompiler::visit(const InlineAssembly &inlineAssembly) {
  solAssert(false, "not implemented yet");
  return false;
}

bool IeleCompiler::visit(const Conditional &condition) {
  // Visit condition.
  iele::IeleValue *ConditionValue = compileExpression(condition.condition());
  solAssert(ConditionValue, "IeleCompiler: failed to compile conditional condition.");

  // The condition target block is the if-true block.
  iele::IeleBlock *CondTargetBlock =
    iele::IeleBlock::Create(&Context, "if.true");

  // Connect the condition block with a conditional jump to the condition target
  // block.
  connectWithConditionalJump(ConditionValue, CompilingBlock, CondTargetBlock);

  // Declare the final result of the conditional.
  iele::IeleLocalVariable *ResultValue =
    iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);

  // Append the expression for the if-false block and assign it to the result.
  iele::IeleValue *FalseValue = compileExpression(condition.falseExpression());
  iele::IeleInstruction::CreateAssign(
    ResultValue, FalseValue, CompilingBlock);

  iele::IeleBlock *IfTrueBlock = CondTargetBlock;

  // Since we have an if-false block, we need a new join block to jump to.
  iele::IeleBlock *JoinBlock = iele::IeleBlock::Create(&Context, "if.end");

  connectWithUnconditionalJump(CompilingBlock, JoinBlock);

  // Add the if-true block at the end of the function and generate its code.
  IfTrueBlock->insertInto(CompilingFunction);
  CompilingBlock = IfTrueBlock;

  // Append the expression for the if-true block and assign it to the result.
  iele::IeleValue *TrueValue = compileExpression(condition.trueExpression());
  iele::IeleInstruction::CreateAssign(
    ResultValue, TrueValue, CompilingBlock);

  // Add the join block at the end of the function and compilation continues in
  // the join block.
  JoinBlock->insertInto(CompilingFunction);
  CompilingBlock = JoinBlock;
  CompilingExpressionResult.push_back(ResultValue);
  return false;
}

bool IeleCompiler::visit(const Assignment &assignment) {
  Token::Value op = assignment.assignmentOperator();
  const Expression &LHS = assignment.leftHandSide();
  const Expression &RHS = assignment.rightHandSide();
  solAssert(LHS.annotation().type->category() != Type::Category::Tuple,
            "not implemented yet");

  // Visit RHS.
  iele::IeleValue *RHSValue = compileExpression(RHS);
  solAssert(RHSValue, "IeleCompiler: Failed to compile RHS of assignment");

  // Visit LHS.
  iele::IeleValue *LHSValue = compileLValue(LHS);
  solAssert(LHSValue, "IeleCompiler: Failed to compile LHS of assignment");

  // Check if we need to do a memory or storage copy.
  TypePointer LHSType = LHS.annotation().type;
  TypePointer RHSType = RHS.annotation().type;
  if (shouldCopyStorageToStorage(LHSValue, RHSType))
    RHSValue = appendIeleRuntimeStorageToStorageCopy(RHSValue);
  else if (shouldCopyMemoryToStorage(LHSType, RHSType))
    RHSValue = appendIeleRuntimeMemoryToStorageCopy(RHSValue);
  else if (shouldCopyStorageToMemory(LHSType, RHSType))
    RHSValue = appendIeleRuntimeStorageToMemoryCopy(RHSValue);

  // Check for compound assignment.
  if (op != Token::Assign) {
    Token::Value binOp = Token::AssignmentToBinaryOp(op);
    iele::IeleValue *LHSDeref = appendLValueDereference(LHSValue);
    RHSValue = appendBinaryOperator(binOp, LHSDeref, RHSValue);
  }

  // Generate assignment code.
  appendLValueAssign(LHSValue, RHSValue);

  // The result of the expression is the RHS.
  CompilingExpressionResult.push_back(RHSValue);
  return false;
}

bool IeleCompiler::visit(const TupleExpression &tuple) {
  if (tuple.components().size() == 1) {
    tuple.components()[0].get()->accept(*this);
  } else {
    solAssert(false, "not implemented yet");
  }
  return false;
}

bool IeleCompiler::visit(const UnaryOperation &unaryOperation) {
  Token::Value UnOperator = unaryOperation.getOperator();
  switch (UnOperator) {
  case Token::Not: {// !
    // Visit subexpression.
    iele::IeleValue *SubExprValue =
      compileExpression(unaryOperation.subExpression());
    solAssert(SubExprValue, "IeleCompiler: Failed to compile operand.");
    // Compile as an iszero.
    iele::IeleLocalVariable *Result =
      iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
    iele::IeleInstruction::CreateIsZero(Result, SubExprValue, CompilingBlock);
    CompilingExpressionResult.push_back(Result);
    break;
  }
  case Token::Add: { // +
    // Visit subexpression.
    iele::IeleValue *SubExprValue =
      compileExpression(unaryOperation.subExpression());
    solAssert(SubExprValue, "IeleCompiler: Failed to compile operand.");
    // unary add, so basically no-op
    CompilingExpressionResult.push_back(SubExprValue);
    break;
  }
  case Token::Sub: { // -
    // Visit subexpression.
    iele::IeleValue *SubExprValue =
      compileExpression(unaryOperation.subExpression());
    solAssert(SubExprValue, "IeleCompiler: Failed to compile operand.");
    // Compile as a subtraction from zero.
    iele::IeleLocalVariable *Result =
      iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
    iele::IeleInstruction::CreateBinOp(iele::IeleInstruction::Sub,
                                       Result,
                                       iele::IeleIntConstant::getZero(&Context),
                                       SubExprValue, CompilingBlock);
    CompilingExpressionResult.push_back(Result);
    break;
  }
  case Token::Inc:    // ++ (pre- or postfix)
  case Token::Dec:  { // -- (pre- or postfix)
    Token::Value BinOperator =
      (UnOperator == Token::Inc) ? Token::Add : Token::Sub;
    // Compile subexpression as an lvalue.
    iele::IeleValue *SubExprValue =
      compileLValue(unaryOperation.subExpression());
    solAssert(SubExprValue, "IeleCompiler: Failed to compile operand.");
    // Get the current subexpression value, and save it in case of a postfix
    // oparation.
    iele::IeleValue *Before = appendLValueDereference(SubExprValue);
    iele::IeleLocalVariable *Result = nullptr;
    if (!unaryOperation.isPrefixOperation()) {
      Result =
        iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
      iele::IeleInstruction::CreateAssign(Result, Before, CompilingBlock);
    }

    // Generate code for the inc/dec operation.
    iele::IeleIntConstant *One = iele::IeleIntConstant::getOne(&Context);
    iele::IeleLocalVariable *After =
      appendBinaryOperator(BinOperator, Before, One);
    // In case of a prefix operation, this is the result.
    if (unaryOperation.isPrefixOperation())
      Result = After;
    // Generate assignment code.
    appendLValueAssign(SubExprValue, After);
    // Return result.
    CompilingExpressionResult.push_back(Result);
    break;
  }
  case Token::BitNot: // ~
  case Token::Delete: // delete
    solAssert(false, "not implemented yet");
    break;
  default:
    solAssert(false, "IeleCompiler: Invalid unary operator");
    break;
  }

  return false;
}

bool IeleCompiler::visit(const BinaryOperation &binaryOperation) {
  // Visit operands.
  iele::IeleValue *LeftOperandValue = 
    compileExpression(binaryOperation.leftExpression());
  solAssert(LeftOperandValue, "IeleCompiler: Failed to compile left operand.");
  iele::IeleValue *RightOperandValue = 
    compileExpression(binaryOperation.rightExpression());
  solAssert(RightOperandValue, "IeleCompiler: Failed to compile right operand.");

  // Append the IELE code for the binary operator.
  iele::IeleValue *Result =
    appendBinaryOperator(binaryOperation.getOperator(),
                         LeftOperandValue, RightOperandValue);

  CompilingExpressionResult.push_back(Result);
  return false;
}

bool IeleCompiler::visit(const FunctionCall &functionCall) {
  // Not supported yet cases.
  if (functionCall.annotation().kind == FunctionCallKind::TypeConversion) {
    solAssert(false, "not implemented yet");
    return false;
  }

  if (!functionCall.names().empty()) {
    solAssert(false, "not implemented yet");
    return false;
  }

  const std::vector<ASTPointer<const Expression>> &arguments =
    functionCall.arguments();

  if (functionCall.annotation().kind ==
        FunctionCallKind::StructConstructorCall) {
    const TypeType &type =
      dynamic_cast<const TypeType &>(
          *functionCall.expression().annotation().type);
    const StructType &structType =
      dynamic_cast<const StructType &>(*type.actualType());
    FunctionTypePointer functionType = structType.constructorType();

    solAssert(arguments.size() == functionType->parameterTypes().size(),
           "IeleCompiler: struct constructor called with wrong number "
           "of arguments");
    solAssert(arguments.size() > 0, "IeleCompiler: empty struct found");
    solAssert(functionType->parameterTypes().size() ==
              structType.structDefinition().members().size(),
              "not implemented yet");

    // Allocate memory for the struct.
    iele::IeleValue *StructValue =
      appendIeleRuntimeAllocateMemory(
        iele::IeleIntConstant::Create(&Context,
                                      llvm::APInt(64, arguments.size())));

    // Visit arguments and initialize struct fields.
    for (unsigned i = 0; i < arguments.size(); ++i) {
      iele::IeleValue *InitValue = compileExpression(*arguments[i]);
      iele::IeleValue *AddressValue =
      appendIeleRuntimeMemoryAddress(
          StructValue,
          iele::IeleIntConstant::Create(&Context, llvm::APInt(64, i)));
      iele::IeleInstruction::CreateSStore(InitValue, AddressValue,
                                          CompilingBlock);
    }

    // Return pointer to allocated struct.
    CompilingExpressionResult.push_back(StructValue);

    return false;
  }

  FunctionTypePointer functionType =
    std::dynamic_pointer_cast<const FunctionType>(
        functionCall.expression().annotation().type);

  const FunctionType &function = *functionType;

  switch (function.kind()) {
  case FunctionType::Kind::Send:
  case FunctionType::Kind::Transfer: {
    // Get target address.
    iele::IeleValue *TargetAddressValue =
      compileExpression(functionCall.expression());
    solAssert(TargetAddressValue,
           "IeleCompiler: Failed to compile transfer target address.");

    // Get transfer value.
    iele::IeleValue *TransferValue =
      compileExpression(*arguments.front().get());
    solAssert(TransferValue,
           "IeleCompiler: Failed to compile transfer value.");

    llvm::SmallVector<iele::IeleValue *, 0> EmptyArguments;
    llvm::SmallVector<iele::IeleLocalVariable *, 0> EmptyLValues;

    // Create computation for gas
    iele::IeleLocalVariable *GasValue =
      iele::IeleLocalVariable::Create(&Context, "gas", CompilingFunction);
    iele::IeleInstruction::CreateIntrinsicCall(
       iele::IeleInstruction::Gaslimit, GasValue, EmptyArguments,
       CompilingBlock);

    // Create call to deposit.
    iele::IeleGlobalVariable *Deposit =
      iele::IeleGlobalVariable::Create(&Context, "deposit");
    iele::IeleLocalVariable *StatusValue =
      iele::IeleLocalVariable::Create(&Context, "status", CompilingFunction);
    iele::IeleInstruction::CreateAccountCall(StatusValue, EmptyLValues, Deposit,
                                             TargetAddressValue,
                                             TransferValue, GasValue,
                                             EmptyArguments,
                                             CompilingBlock);
    
    CompilingExpressionResult.push_back(StatusValue);

    if (function.kind() == FunctionType::Kind::Transfer) {
      // For transfer revert if status is not zero.
      appendRevert(StatusValue);
    }
    break;
  }
  case FunctionType::Kind::Revert:
      appendRevert();
      break;
  case FunctionType::Kind::Assert:
  case FunctionType::Kind::Require: {
    // Visit condition.
    iele::IeleValue *ConditionValue =
      compileExpression(*arguments.front().get());
    solAssert(ConditionValue,
           "IeleCompiler: Failed to compile require/assert condition.");

    // Create check for false.
    iele::IeleLocalVariable *InvConditionValue =
      iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
    iele::IeleInstruction::CreateIsZero(InvConditionValue, ConditionValue,
                                        CompilingBlock);
    if (function.kind() == FunctionType::Kind::Assert)
      appendInvalid(InvConditionValue);
    else
      appendRevert(InvConditionValue);
    break;
  }
  case FunctionType::Kind::AddMod: {
    iele::IeleValue *Op1 = compileExpression(*arguments[0].get());
    solAssert(Op1,
           "IeleCompiler: Failed to compile operand 1 of addmod.");
    iele::IeleValue *Op2 = compileExpression(*arguments[1].get());
    solAssert(Op2,
           "IeleCompiler: Failed to compile operand 2 of addmod.");
    iele::IeleValue *Modulus = compileExpression(*arguments[2].get());
    solAssert(Modulus,
           "IeleCompiler: Failed to compile modulus of addmod.");

    iele::IeleLocalVariable *AddModValue =
      iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
    iele::IeleInstruction::CreateTernOp(iele::IeleInstruction::AddMod,
                                        AddModValue, Op1, Op2, Modulus, 
                                        CompilingBlock);
    CompilingExpressionResult.push_back(AddModValue);
    break;
  }
  case FunctionType::Kind::MulMod: {
    iele::IeleValue *Op1 = compileExpression(*arguments[0].get());
    solAssert(Op1,
           "IeleCompiler: Failed to compile operand 1 of addmod.");
    iele::IeleValue *Op2 = compileExpression(*arguments[1].get());
    solAssert(Op2,
           "IeleCompiler: Failed to compile operand 2 of addmod.");
    iele::IeleValue *Modulus = compileExpression(*arguments[2].get());
    solAssert(Modulus,
           "IeleCompiler: Failed to compile modulus of addmod.");

    iele::IeleLocalVariable *MulModValue =
      iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
    iele::IeleInstruction::CreateTernOp(iele::IeleInstruction::MulMod,
                                        MulModValue, Op1, Op2, Modulus, 
                                        CompilingBlock);
    CompilingExpressionResult.push_back(MulModValue);
    break;
  }
  case FunctionType::Kind::ObjectCreation: {
    solAssert(arguments.size() == 1,
              "IeleCompiler: invalid arguments for array creation");

    // Visit argument to get the requested size.
    iele::IeleValue *ArraySizeValue = compileExpression(*arguments[0]);

    // Allocate memory for the array.
    iele::IeleValue *ArrayValue =
      appendIeleRuntimeAllocateMemory(ArraySizeValue);

    // Return pointer to allocated array.
    CompilingExpressionResult.push_back(ArrayValue);

    return false;
  }
  case FunctionType::Kind::Internal:
  case FunctionType::Kind::External:
  case FunctionType::Kind::CallCode:
  case FunctionType::Kind::DelegateCall:
  case FunctionType::Kind::BareCall:
  case FunctionType::Kind::BareCallCode:
  case FunctionType::Kind::BareDelegateCall:
  case FunctionType::Kind::Creation:
  case FunctionType::Kind::SetGas:
  case FunctionType::Kind::SetValue:
  case FunctionType::Kind::ByteArrayPush:
  case FunctionType::Kind::ArrayPush:
  case FunctionType::Kind::Log0:
  case FunctionType::Kind::Log1:
  case FunctionType::Kind::Log2:
  case FunctionType::Kind::Log3:
  case FunctionType::Kind::Log4:
  case FunctionType::Kind::Event:
  case FunctionType::Kind::Selfdestruct:
  case FunctionType::Kind::SHA3:
  case FunctionType::Kind::BlockHash:
  case FunctionType::Kind::ECRecover:
  case FunctionType::Kind::SHA256:
  case FunctionType::Kind::RIPEMD160:
    solAssert(false, "not implemented yet");
    break;
  default:
      solAssert(false, "IeleCompiler: Invalid function type.");
  }

  return false;
}

bool IeleCompiler::visit(const NewExpression &newExpression) {
  solAssert(false, "not implemented yet");
  return false;
}

bool IeleCompiler::visit(const MemberAccess &memberAccess) {
   const std::string& member = memberAccess.memberName();

  // Not supported yet cases.
  if (const FunctionType *funcType =
        dynamic_cast<const FunctionType *>(
            memberAccess.annotation().type.get())) {
    if (funcType->bound()) {
      solAssert(false, "not implemented yet");
      return false;
    }
  }

  if (dynamic_cast<const TypeType *>(
          memberAccess.expression().annotation().type.get())) {
    solAssert(false, "not implemented yet");
    return false;
  }

  const Type &baseType = *memberAccess.expression().annotation().type;

  // Visit accessed exression (skip in case of magic base expression).
  iele::IeleValue *ExprValue = nullptr;
  if (baseType.category() != Type::Category::Magic) {
    ExprValue = compileExpression(memberAccess.expression());
    solAssert(ExprValue,
              "IeleCompiler: failed to compile base expression for member "
              "access.");
  }

  switch (baseType.category()) {
  case Type::Category::Magic:
    // Reserved members of reserved identifiers. We can ignore the kind of magic
    // and only look at the name of the member.
    if (member == "timestamp") {
      llvm::SmallVector<iele::IeleValue *, 0> EmptyArguments;
      iele::IeleLocalVariable *TimestampValue =
        iele::IeleLocalVariable::Create(&Context, "timestamp", CompilingFunction);
      iele::IeleInstruction::CreateIntrinsicCall(
        iele::IeleInstruction::Timestamp, TimestampValue, EmptyArguments,
        CompilingBlock);
      CompilingExpressionResult.push_back(TimestampValue);
    } else if (member == "difficulty") {
      llvm::SmallVector<iele::IeleValue *, 0> EmptyArguments;
      iele::IeleLocalVariable *DifficultyValue =
        iele::IeleLocalVariable::Create(&Context, "difficulty", CompilingFunction);
      iele::IeleInstruction::CreateIntrinsicCall(
        iele::IeleInstruction::Difficulty, DifficultyValue, EmptyArguments,
        CompilingBlock);
      CompilingExpressionResult.push_back(DifficultyValue);
    } else if (member == "number") {
      llvm::SmallVector<iele::IeleValue *, 0> EmptyArguments;
      iele::IeleLocalVariable *NumberValue =
        iele::IeleLocalVariable::Create(&Context, "number", CompilingFunction);
      iele::IeleInstruction::CreateIntrinsicCall(
        iele::IeleInstruction::Number, NumberValue, EmptyArguments,
        CompilingBlock);
      CompilingExpressionResult.push_back(NumberValue);
    } else if (member == "gaslimit") {
      llvm::SmallVector<iele::IeleValue *, 0> EmptyArguments;
      iele::IeleLocalVariable *GaslimitValue =
        iele::IeleLocalVariable::Create(&Context, "gaslimit", CompilingFunction);
      iele::IeleInstruction::CreateIntrinsicCall(
        iele::IeleInstruction::Gaslimit, GaslimitValue, EmptyArguments,
        CompilingBlock);
      CompilingExpressionResult.push_back(GaslimitValue);
    } else if (member == "sender") {
      llvm::SmallVector<iele::IeleValue *, 0> EmptyArguments;
      iele::IeleLocalVariable *CallerValue =
      iele::IeleLocalVariable::Create(&Context, "caller", CompilingFunction);
      iele::IeleInstruction::CreateIntrinsicCall(
        iele::IeleInstruction::Caller, CallerValue, EmptyArguments,
        CompilingBlock);
      CompilingExpressionResult.push_back(CallerValue);
    } else if (member == "value") {
      llvm::SmallVector<iele::IeleValue *, 0> EmptyArguments;
      iele::IeleLocalVariable *CallvalueValue =
        iele::IeleLocalVariable::Create(&Context, "callvalue", CompilingFunction);
      iele::IeleInstruction::CreateIntrinsicCall(
        iele::IeleInstruction::Callvalue, CallvalueValue, EmptyArguments,
        CompilingBlock);
      CompilingExpressionResult.push_back(CallvalueValue);
    } else if (member == "origin") {
      llvm::SmallVector<iele::IeleValue *, 0> EmptyArguments;
      iele::IeleLocalVariable *OriginValue =
        iele::IeleLocalVariable::Create(&Context, "origin", CompilingFunction);
      iele::IeleInstruction::CreateIntrinsicCall(
        iele::IeleInstruction::Origin, OriginValue, EmptyArguments,
        CompilingBlock);
      CompilingExpressionResult.push_back(OriginValue);
    } else if (member == "gas") {
      llvm::SmallVector<iele::IeleValue *, 0> EmptyArguments;
      iele::IeleLocalVariable *GasValue =
        iele::IeleLocalVariable::Create(&Context, "gas", CompilingFunction);
      iele::IeleInstruction::CreateIntrinsicCall(
        iele::IeleInstruction::Gas, GasValue, EmptyArguments,
        CompilingBlock);
      CompilingExpressionResult.push_back(GasValue);
    } else if (member == "gasprice") {
      llvm::SmallVector<iele::IeleValue *, 0> EmptyArguments;
      iele::IeleLocalVariable *GaspriceValue =
        iele::IeleLocalVariable::Create(&Context, "gasprice", CompilingFunction);
      iele::IeleInstruction::CreateIntrinsicCall(
        iele::IeleInstruction::Gasprice, GaspriceValue, EmptyArguments,
        CompilingBlock);
      CompilingExpressionResult.push_back(GaspriceValue);
    } else if (member == "coinbase") {
      llvm::SmallVector<iele::IeleValue *, 0> EmptyArguments;
      iele::IeleLocalVariable *BeneficiaryValue =
        iele::IeleLocalVariable::Create(&Context, "beneficiary", CompilingFunction);
      iele::IeleInstruction::CreateIntrinsicCall(
        iele::IeleInstruction::Beneficiary, BeneficiaryValue, EmptyArguments,
        CompilingBlock);
      CompilingExpressionResult.push_back(BeneficiaryValue);
    } else if (member == "data")
      solAssert(false, "IeleCompiler: member not supported in IELE");
    else if (member == "sig")
      solAssert(false, "IeleCompiler: member not supported in IELE");
    else
      solAssert(false, "IeleCompiler: Unknown magic member.");
    break;
  case Type::Category::Integer: {
    solAssert(!(std::set<std::string>{"call", "callcode", "delegatecall"}).count(member),
              "IeleCompiler: member not supported in IELE");
    if (member == "transfer" || member == "send") {
      // Simply forward the result (which should be an address).
      solAssert(ExprValue, "IeleCompiler: Failed to compile address");
      CompilingExpressionResult.push_back(ExprValue);
    } else if (member == "balance") {
      llvm::SmallVector<iele::IeleValue *, 0> Arguments(1, ExprValue);
      iele::IeleLocalVariable *BalanceValue =
        iele::IeleLocalVariable::Create(&Context, "balance", CompilingFunction);
      iele::IeleInstruction::CreateIntrinsicCall(
        iele::IeleInstruction::Balance, BalanceValue, Arguments,
        CompilingBlock);
      CompilingExpressionResult.push_back(BalanceValue);
    } else
      solAssert(false, "IeleCompiler: invalid member for integer value");
    break;
  }
  case Type::Category::Function:
    if (member == "selector")
      solAssert(false, "IeleCompiler: member not supported in IELE");
    else
      solAssert(false, "IeleCompiler: invalid member for function value");
    break;
  case Type::Category::Struct: {
    const StructType &type = dynamic_cast<const StructType &>(baseType);
    iele::IeleValue *OffsetValue =
      iele::IeleIntConstant::Create(
          &Context,
          llvm::APInt(64, getStructMemberIndex(type, member)));
    switch (type.location()) {
    case DataLocation::Storage: {
      if (CompilingLValue) {
        iele::IeleValue *AddressValue =
          appendIeleRuntimeStorageAddress(ExprValue, OffsetValue);
        CompilingExpressionResult.push_back(AddressValue);
        CompilingLValueKind = LValueKind::Storage;
      } else {
        iele::IeleValue *LoadedValue =
          appendIeleRuntimeStorageLoad(ExprValue, OffsetValue);
        CompilingExpressionResult.push_back(LoadedValue);
      }
      break;
    }
    case DataLocation::Memory: {
      if (CompilingLValue) {
        iele::IeleValue *AddressValue =
          appendIeleRuntimeMemoryAddress(ExprValue, OffsetValue);
        CompilingExpressionResult.push_back(AddressValue);
        CompilingLValueKind = LValueKind::Memory;
      } else {
        iele::IeleValue *LoadedValue =
          appendIeleRuntimeMemoryLoad(ExprValue, OffsetValue);
        CompilingExpressionResult.push_back(LoadedValue);
      }
      break;
    }
    default:
      solAssert(false, "IeleCompiler: Illegal data location for struct.");
    }
    break;
  }
  case Type::Category::Array:
  case Type::Category::Contract:
  case Type::Category::Enum:
  case Type::Category::FixedBytes:
    solAssert(false, "not implemented yet");
  default:
    solAssert(false, "IeleCompiler: Member access to unknown type.");
  }

  return false;
}

bool IeleCompiler::visit(const IndexAccess &indexAccess) {
  // Visit accessed exression.
  iele::IeleValue *ExprValue = compileExpression(indexAccess.baseExpression());
  solAssert(ExprValue,
            "IeleCompiler: failed to compile base expression for index "
            "access.");

  const Type &baseType = *indexAccess.baseExpression().annotation().type;
  switch (baseType.category()) {
  case Type::Category::Array: {
    const ArrayType &type = dynamic_cast<const ArrayType &>(baseType);

    solAssert(indexAccess.indexExpression(),
              "IeleCompiler: Index expression expected.");

    // Visit index expression.
    iele::IeleValue *IndexValue =
      compileExpression(*indexAccess.indexExpression());
    solAssert(IndexValue,
              "IeleCompiler: failed to compile index expression for index "
              "access.");
    solAssert(!type.isByteArray(), "not implemented yet");
    switch (type.location()) {
    case DataLocation::Storage: {
      if (CompilingLValue) {
        iele::IeleValue *AddressValue =
          appendIeleRuntimeStorageAddress(ExprValue, IndexValue);
        CompilingExpressionResult.push_back(AddressValue);
        CompilingLValueKind = LValueKind::Storage;
      } else {
        iele::IeleValue *LoadedValue =
          appendIeleRuntimeStorageLoad(ExprValue, IndexValue);
        CompilingExpressionResult.push_back(LoadedValue);
      }
      break;
    }
    case DataLocation::Memory: {
      if (CompilingLValue) {
        iele::IeleValue *AddressValue =
          appendIeleRuntimeMemoryAddress(ExprValue, IndexValue);
        CompilingExpressionResult.push_back(AddressValue);
        CompilingLValueKind = LValueKind::Memory;
      } else {
        iele::IeleValue *LoadedValue =
          appendIeleRuntimeMemoryLoad(ExprValue, IndexValue);
        CompilingExpressionResult.push_back(LoadedValue);
      }
      break;
    }
    case DataLocation::CallData:
      solAssert(false, "not implemented yet.");
    }
    break;
  }
  case Type::Category::Mapping:
  case Type::Category::FixedBytes:
  case Type::Category::TypeType:
    solAssert(false, "not implemented yet");
  default:
    solAssert(false, "IeleCompiler: Index access to unknown type.");
  }

  return false;
}

void IeleCompiler::endVisit(const Identifier &identifier) {
  const std::string &name = identifier.name();

  // Check if identifier is a reserved identifier.
  const Declaration *declaration =
    identifier.annotation().referencedDeclaration;
  if (const MagicVariableDeclaration *magicVar =
         dynamic_cast<const MagicVariableDeclaration *>(declaration)) {
    switch (magicVar->type()->category()) {
    case Type::Category::Contract:
      // Reserved identifiers: "this" or "super"
      solAssert(false, "not implemented yet");
      return;
    case Type::Category::Integer: {
      // Reserved identifier: now.
      llvm::SmallVector<iele::IeleValue *, 0> EmptyArguments;
      iele::IeleLocalVariable *TimestampValue =
      iele::IeleLocalVariable::Create(&Context, "timestamp", CompilingFunction);
      iele::IeleInstruction::CreateIntrinsicCall(
        iele::IeleInstruction::Timestamp, TimestampValue, EmptyArguments,
        CompilingBlock);
      CompilingExpressionResult.push_back(TimestampValue);
      return;
    }
    default:
      // The rest of reserved identifiers appear only as part of a member access
      // expression and are handled there.
      return;
    }
  }

  // Lookup identifier in the function's symbol table.
  iele::IeleValueSymbolTable *ST = CompilingFunction->getIeleValueSymbolTable();
  solAssert(ST,
            "IeleCompiler: failed to access compiling function's symbol "
            "table.");
  if (iele::IeleValue *Identifier =
        ST->lookup(VarNameMap[ModifierDepth][name])) {
    if (CompilingLValue)
      CompilingLValueKind = LValueKind::Reg;
    CompilingExpressionResult.push_back(Identifier);
    return;
  }

  // If not found, lookup identifier in the contract's symbol table.
  ST = CompilingContract->getIeleValueSymbolTable();
  solAssert(ST,
            "IeleCompiler: failed to access compiling contract's symbol "
            "table.");
  if (iele::IeleValue *Identifier = ST->lookup(name)) {
    if (iele::IeleGlobalVariable *GV =
          llvm::dyn_cast<iele::IeleGlobalVariable>(Identifier)) {
      // In case of a global variable, if we aren't compiling an lvalue, we have
      // to load the global variable.
      if (!CompilingLValue) {
        iele::IeleLocalVariable *LoadedValue =
          iele::IeleLocalVariable::Create(&Context, name + ".val",
                                          CompilingFunction);
        iele::IeleInstruction::CreateSLoad(LoadedValue, GV, CompilingBlock);
        CompilingExpressionResult.push_back(LoadedValue);
        return;
      } else {
        CompilingLValueKind = LValueKind::Storage;
      }
    } else if (CompilingLValue) {
      CompilingLValueKind = LValueKind::Reg;
    }

    CompilingExpressionResult.push_back(Identifier);
    return;
  }

  // If not found, make a new IeleLocalVariable for the identifier.
  iele::IeleLocalVariable *Identifier =
    iele::IeleLocalVariable::Create(&Context, name, CompilingFunction);
  if (CompilingLValue)
    CompilingLValueKind = LValueKind::Reg;
  CompilingExpressionResult.push_back(Identifier);
  return;
}

void IeleCompiler::endVisit(const Literal &literal) {
  TypePointer type = literal.annotation().type;
  switch (type->category()) {
  case Type::Category::Bool:
  case Type::Category::RationalNumber:
  case Type::Category::Integer: {
    iele::IeleIntConstant *LiteralValue =
      iele::IeleIntConstant::Create(
          &Context,
          llvm::APInt(64, (uint64_t) type->literalValue(&literal), true));
    CompilingExpressionResult.push_back(LiteralValue);
    break;
  }
  case Type::Category::StringLiteral:
  default:
    solAssert(false, "not implemented yet");
    break;
  }
}

iele::IeleValue *IeleCompiler::compileExpression(const Expression &expression) {
  // Save current expression compilation status.
  bool SavedCompilingLValue = CompilingLValue;

  // Visit expression.
  CompilingLValue = false;
  expression.accept(*this);

  // Expression visitors should store the value that is the result of the
  // compiled for the expression computation in the CompilingExpressionResult
  // field. This helper should only be used when a scalar value (or void) is
  // expected as the result of the corresponding expression computation.
  iele::IeleValue *Result = nullptr;
  if (CompilingExpressionResult.size() > 0)
    Result = CompilingExpressionResult[0];

  // Restore expression compilation status and return result.
  CompilingExpressionResult.clear();
  CompilingLValue = SavedCompilingLValue;
  return Result;
}

void IeleCompiler::compileTuple(
    const Expression &expression,
    llvm::SmallVectorImpl<iele::IeleValue *> &Result) {
  // Save current expression compilation status.
  bool SavedCompilingLValue = CompilingLValue;

  // Visit expression.
  CompilingLValue = false;
  expression.accept(*this);

  // Expression visitors should store the value that is the result of the
  // compiled for the expression computation in the CompilingExpressionResult
  // field. This helper should only be used when tupple value is expected as
  // the result of the corresponding expression computation.
  solAssert(CompilingExpressionResult.size() > 0, "expression visitor did not set a result value");
  Result.insert(Result.end(),
                CompilingExpressionResult.begin(),
                CompilingExpressionResult.end());

  // Restore expression compilation status and return result.
  CompilingExpressionResult.clear();
  CompilingLValue = SavedCompilingLValue;
}

iele::IeleValue *IeleCompiler::compileLValue(const Expression &expression) {
  // Save current expression compilation status.
  bool SavedCompilingLValue = CompilingLValue;

  // Visit expression as LValue.
  CompilingLValue = true;
  expression.accept(*this);

  // Expression visitors should store the value that is the result of the
  // compiled for the expression computation in the CompilingExpressionResult
  // field. This helper should only be used when a scalar lvalue is expected as
  // the result of the corresponding expression computation.
  iele::IeleValue *Result = nullptr;
  solAssert(CompilingExpressionResult.size() > 0,
            "IeleCompiler: Expression visitor did not set a result value");
  Result = CompilingExpressionResult[0];
  solAssert(llvm::isa<iele::IeleLocalVariable>(Result) ||
            llvm::isa<iele::IeleGlobalVariable>(Result),
            "IeleCompiler: Invalid compilation for an lvalue");

  // Restore expression compilation status and return result.
  CompilingExpressionResult.clear();
  CompilingLValue = SavedCompilingLValue;
  return Result;
}

void IeleCompiler::connectWithUnconditionalJump(
    iele::IeleBlock *SourceBlock, iele::IeleBlock *DestinationBlock) {
  iele::IeleInstruction::CreateUncondBr(DestinationBlock, SourceBlock);
}

void IeleCompiler::connectWithConditionalJump(
    iele::IeleValue *Condition, iele::IeleBlock *SourceBlock, 
    iele::IeleBlock *DestinationBlock) {
  iele::IeleInstruction::CreateCondBr(Condition, DestinationBlock,
                                      SourceBlock);
}

void IeleCompiler::appendRevert(iele::IeleValue *Condition) {
  // Create the revert block if it's not already created.
  if (!RevertBlock) {
    RevertBlock = iele::IeleBlock::Create(&Context, "throw");
    iele::IeleInstruction::CreateRevert(
        iele::IeleIntConstant::getMinusOne(&Context), RevertBlock);
  }

  if (Condition)
    connectWithConditionalJump(Condition, CompilingBlock, RevertBlock);
  else
    connectWithUnconditionalJump(CompilingBlock, RevertBlock);
}

void IeleCompiler::appendInvalid(iele::IeleValue *Condition) {
  // Create the assert-fail block if it's not already created.
  if (!AssertFailBlock) {
    AssertFailBlock = iele::IeleBlock::Create(&Context, "invalid");
    solAssert(false, "not implemented yet");
  }

  if (Condition)
    connectWithConditionalJump(Condition, CompilingBlock, AssertFailBlock);
  else
    connectWithUnconditionalJump(CompilingBlock, AssertFailBlock);
}

void IeleCompiler::appendStateVariableInitialization() {
  for (const VariableDeclaration *stateVariable :
         CompilingContractASTNode->stateVariables()) {
    // Visit initialization value if it exists.
    iele::IeleValue *InitValue = nullptr;
    TypePointer type = stateVariable->annotation().type;
    if (stateVariable->value())
      InitValue = compileExpression(*stateVariable->value());
    // Else, check if we need to allocate storage memory for a reference type.
    else if (type->dataStoredIn(DataLocation::Storage)) {
      iele::IeleValue *SizeValue;
      if (type->category() == Type::Category::Array) {
        SizeValue = iele::IeleIntConstant::getZero(&Context);
      } else {
        solAssert(type->category() == Type::Category::Struct,
                  "not implemented yet");
        const StructType &structType =
          dynamic_cast<const StructType &>(*type);
        SizeValue =
          iele::IeleIntConstant::Create(
              &Context,
              llvm::APInt(64,
                          structType.structDefinition().members().size()));
      }
      InitValue = appendIeleRuntimeAllocateStorage(SizeValue);
    }

    if (!InitValue)
      continue;

    // Lookup the state variable name in the contract's symbol table.
    iele::IeleValueSymbolTable *ST =
      CompilingContract->getIeleValueSymbolTable();
    solAssert(ST,
              "IeleCompiler: failed to access compiling contract's symbol "
              "table.");
    iele::IeleValue *LHSValue = ST->lookup(stateVariable->name());
    solAssert(LHSValue, "IeleCompiler: Failed to compile LHS of state variable"
                        " initialization");

    // Add assignment in constructor's body.
    iele::IeleInstruction::CreateSStore(InitValue, LHSValue, CompilingBlock);
  }
}

iele::IeleLocalVariable *IeleCompiler::appendIeleRuntimeAllocateMemory(
     iele::IeleValue *NumElems) {
  iele::IeleLocalVariable *Result =
    iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
  iele::IeleGlobalVariable *Callee =
    iele::IeleGlobalVariable::Create(&Context, "iele.memory.allocate");
  llvm::SmallVector<iele::IeleLocalVariable *, 1> Results(1, Result);
  llvm::SmallVector<iele::IeleValue *, 1> Arguments(1, NumElems);
  iele::IeleInstruction::CreateInternalCall(Results, Callee, Arguments,
                                            CompilingBlock);
  return Result;
}

iele::IeleLocalVariable *IeleCompiler::appendIeleRuntimeAllocateStorage(
     iele::IeleValue *NumElems) {
  iele::IeleLocalVariable *Result =
    iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
  iele::IeleGlobalVariable *Callee =
    iele::IeleGlobalVariable::Create(&Context, "iele.storage.allocate");
  llvm::SmallVector<iele::IeleLocalVariable *, 1> Results(1, Result);
  llvm::SmallVector<iele::IeleValue *, 1> Arguments(1, NumElems);
  iele::IeleInstruction::CreateInternalCall(Results, Callee, Arguments,
                                            CompilingBlock);
  return Result;
}

iele::IeleLocalVariable *IeleCompiler::appendIeleRuntimeMemoryAddress(
     iele::IeleValue *Base, iele::IeleValue *Offset) {
  iele::IeleLocalVariable *Result =
    iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
  iele::IeleGlobalVariable *Callee =
    iele::IeleGlobalVariable::Create(&Context, "iele.memory.address");
  llvm::SmallVector<iele::IeleLocalVariable *, 1> Results(1, Result);
  llvm::SmallVector<iele::IeleValue *, 2> Arguments;
  Arguments.push_back(Base);
  Arguments.push_back(Offset);
  iele::IeleInstruction::CreateInternalCall(Results, Callee, Arguments,
                                            CompilingBlock);
  return Result;
}

iele::IeleLocalVariable *IeleCompiler::appendIeleRuntimeStorageAddress(
    iele::IeleValue *Base, iele::IeleValue *Offset) {
  iele::IeleLocalVariable *Result =
    iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
  iele::IeleGlobalVariable *Callee =
    iele::IeleGlobalVariable::Create(&Context, "iele.storage.address");
  llvm::SmallVector<iele::IeleLocalVariable *, 1> Results(1, Result);
  llvm::SmallVector<iele::IeleValue *, 2> Arguments;
  Arguments.push_back(Base);
  Arguments.push_back(Offset);
  iele::IeleInstruction::CreateInternalCall(Results, Callee, Arguments,
                                            CompilingBlock);
  return Result;
}

iele::IeleLocalVariable *IeleCompiler::appendIeleRuntimeMemoryLoad(
    iele::IeleValue *Base, iele::IeleValue *Offset) {
  iele::IeleLocalVariable *Result =
    iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
  iele::IeleGlobalVariable *Callee =
    iele::IeleGlobalVariable::Create(&Context, "iele.memory.load");
  llvm::SmallVector<iele::IeleLocalVariable *, 1> Results(1, Result);
  llvm::SmallVector<iele::IeleValue *, 2> Arguments;
  Arguments.push_back(Base);
  Arguments.push_back(Offset);
  iele::IeleInstruction::CreateInternalCall(Results, Callee, Arguments,
                                            CompilingBlock);
  return Result;
}

iele::IeleLocalVariable *IeleCompiler::appendIeleRuntimeStorageLoad(
    iele::IeleValue *Base, iele::IeleValue *Offset) {
  iele::IeleLocalVariable *Result =
    iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
  iele::IeleGlobalVariable *Callee =
    iele::IeleGlobalVariable::Create(&Context, "iele.storage.load");
  llvm::SmallVector<iele::IeleLocalVariable *, 1> Results(1, Result);
  llvm::SmallVector<iele::IeleValue *, 2> Arguments;
  Arguments.push_back(Base);
  Arguments.push_back(Offset);
  iele::IeleInstruction::CreateInternalCall(Results, Callee, Arguments,
                                            CompilingBlock);
  return Result;
}

iele::IeleLocalVariable *IeleCompiler::appendIeleRuntimeStorageToStorageCopy(
    iele::IeleValue *From) {
  iele::IeleLocalVariable *Result =
    iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
  iele::IeleGlobalVariable *Callee =
    iele::IeleGlobalVariable::Create(&Context, "iele.storage.copy.to.storage");
  llvm::SmallVector<iele::IeleLocalVariable *, 1> Results(1, Result);
  llvm::SmallVector<iele::IeleValue *, 1> Arguments(1, From);
  iele::IeleInstruction::CreateInternalCall(Results, Callee, Arguments,
                                            CompilingBlock);
  return Result;
}

iele::IeleLocalVariable *IeleCompiler::appendIeleRuntimeMemoryToStorageCopy(
    iele::IeleValue *From) {
  iele::IeleLocalVariable *Result =
    iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
  iele::IeleGlobalVariable *Callee =
    iele::IeleGlobalVariable::Create(&Context, "iele.memory.copy.to.storage");
  llvm::SmallVector<iele::IeleLocalVariable *, 1> Results(1, Result);
  llvm::SmallVector<iele::IeleValue *, 1> Arguments(1, From);
  iele::IeleInstruction::CreateInternalCall(Results, Callee, Arguments,
                                            CompilingBlock);
  return Result;
}

iele::IeleLocalVariable *IeleCompiler::appendIeleRuntimeStorageToMemoryCopy(
    iele::IeleValue *From) {
  iele::IeleLocalVariable *Result =
    iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
  iele::IeleGlobalVariable *Callee =
    iele::IeleGlobalVariable::Create(&Context, "iele.storage.copy.to.memory");
  llvm::SmallVector<iele::IeleLocalVariable *, 1> Results(1, Result);
  llvm::SmallVector<iele::IeleValue *, 1> Arguments(1, From);
  iele::IeleInstruction::CreateInternalCall(Results, Callee, Arguments,
                                            CompilingBlock);
  return Result;
}

iele::IeleLocalVariable *IeleCompiler::appendLValueDereference(
    iele::IeleValue *LValue) {
  // If the LValue is a pointer to storage (e.g. state variable) we need to
  // perform an sload, if it is a pointer to memory then a load, else
  // dereference is a noop.
  iele::IeleLocalVariable *LoadedValue = nullptr;
  switch (CompilingLValueKind) {
  case LValueKind::Storage: {
    LoadedValue = iele::IeleLocalVariable::Create(
                     &Context, "tmp", CompilingFunction);
    iele::IeleInstruction::CreateSLoad(LoadedValue, LValue, CompilingBlock);
    break;
  }
  case LValueKind::Memory: {
    LoadedValue = iele::IeleLocalVariable::Create(
                     &Context, "tmp", CompilingFunction);
    iele::IeleInstruction::CreateLoad(LoadedValue, LValue, CompilingBlock);
    break;
  }
  case LValueKind::Reg: {
    LoadedValue = llvm::cast<iele::IeleLocalVariable>(LValue);
    break;
  }
  }

  return LoadedValue;
}

void IeleCompiler::appendLValueAssign(iele::IeleValue *LValue,
                                      iele::IeleValue *RValue) {
  // If the LValue is a pointer to storage (e.g. state variable) we need to
  // perform an sstore, if it is a pointer to memory then a store, else a simple
  // assignment.
  switch (CompilingLValueKind) {
  case LValueKind::Storage:
    iele::IeleInstruction::CreateSStore(RValue, LValue, CompilingBlock);
    break;
  case LValueKind::Memory:
    iele::IeleInstruction::CreateStore(RValue, LValue, CompilingBlock);
    break;
  case LValueKind::Reg:
    iele::IeleInstruction::CreateAssign(
        llvm::cast<iele::IeleLocalVariable>(LValue), RValue, CompilingBlock);
    break;
  }
}

iele::IeleLocalVariable *IeleCompiler::appendBinaryOperator(
    Token::Value Opcode,
    iele::IeleValue *LeftOperand,
    iele::IeleValue *RightOperand) {
  // Find corresponding IELE binary opcode.
  iele::IeleInstruction::IeleOps BinOpcode;
  switch (Opcode) {
  case Token::Add:                BinOpcode = iele::IeleInstruction::Add; break;
  case Token::Sub:                BinOpcode = iele::IeleInstruction::Sub; break;
  case Token::Mul:                BinOpcode = iele::IeleInstruction::Mul; break;
  case Token::Div:                BinOpcode = iele::IeleInstruction::Div; break;
  case Token::Mod:                BinOpcode = iele::IeleInstruction::Mod; break;
  case Token::Exp:                BinOpcode = iele::IeleInstruction::Exp; break;
  case Token::Equal:              BinOpcode = iele::IeleInstruction::CmpEq; break;
  case Token::NotEqual:           BinOpcode = iele::IeleInstruction::CmpNe; break;
  case Token::GreaterThanOrEqual: BinOpcode = iele::IeleInstruction::CmpGe; break;
  case Token::LessThanOrEqual:    BinOpcode = iele::IeleInstruction::CmpLe; break;
  case Token::GreaterThan:        BinOpcode = iele::IeleInstruction::CmpGt; break;
  case Token::LessThan:           BinOpcode = iele::IeleInstruction::CmpLt; break;
  default:
    solAssert(false, "not implemented yet");
    solAssert(false, "IeleCompiler: Invalid binary operator");
  }

  // Create the instruction.
  iele::IeleLocalVariable *Result =
    iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
  iele::IeleInstruction::CreateBinOp(
      BinOpcode, Result, LeftOperand, RightOperand, CompilingBlock);

  return Result;
}

bool IeleCompiler::shouldCopyStorageToStorage(iele::IeleValue *To,
                                              TypePointer From) const {
  return llvm::isa<iele::IeleGlobalVariable>(To) &&
         From->dataStoredIn(DataLocation::Storage);
}

bool IeleCompiler::shouldCopyMemoryToStorage(TypePointer To,
                                             TypePointer From) const {
  return To->dataStoredIn(DataLocation::Storage) &&
         From->dataStoredIn(DataLocation::Memory);
}

bool IeleCompiler::shouldCopyStorageToMemory(TypePointer To,
                                             TypePointer From) const {
  return To->dataStoredIn(DataLocation::Memory) &&
         From->dataStoredIn(DataLocation::Storage);
}

unsigned IeleCompiler::getStructMemberIndex(const StructType &type,
                                            const std::string &member) const {
  unsigned Index = 0;
  for (const auto memberDecl : type.structDefinition().members()) {
    if (member == memberDecl->name())
      return Index;
    ++Index;
  }

  solAssert(false, "IeleCompiler: look up for non-existing member");
  return 0;
}
