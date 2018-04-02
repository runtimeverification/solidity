#include "libsolidity/codegen/IeleCompiler.h"

#include <libsolidity/interface/Exceptions.h>

#include "libiele/IeleContract.h"
#include "libiele/IeleGlobalVariable.h"
#include "libiele/IeleIntConstant.h"

#include <iostream>
#include "llvm/Support/raw_ostream.h"

using namespace dev;
using namespace dev::solidity;

const IntegerType &SInt = IntegerType(256, IntegerType::Modifier::Signed);
const IntegerType &Address = IntegerType(160, IntegerType::Modifier::Address);

std::string IeleCompiler::getIeleNameForFunction(
    const FunctionDefinition &function) {
  std::string IeleFunctionName;
  if (function.isConstructor())
    IeleFunctionName = "init";
  else if (function.isFallback())
    IeleFunctionName = "deposit";
  else if (function.isPublic())
    IeleFunctionName = function.externalSignature();
  else
    // TODO: overloading on internal functions.
    IeleFunctionName = function.name();

  if (isMostDerived(&function)) {
    return IeleFunctionName;
  } else {
    return contractFor(&function)->name() + "." + IeleFunctionName;
  }
}

std::string IeleCompiler::getIeleNameForStateVariable(
    const VariableDeclaration *stateVariable) {
  std::string IeleVariableName;
  if (isMostDerived(stateVariable)) {
    return stateVariable->name();
  } else {
    return contractFor(stateVariable)->name() + "." + stateVariable->name();
  }
}

// lookup a ModifierDefinition by name (borrowed from CompilerContext.cpp)
const ModifierDefinition &IeleCompiler::functionModifier(
    const std::string &_name) const {
  solAssert(!CompilingContractInheritanceHierarchy.empty(), "CurrentContract not set.");
  for (const ContractDefinition *CurrentContract : CompilingContractInheritanceHierarchy) {
    for (ModifierDefinition const* modifier: CurrentContract->functionModifiers())
      if (modifier->name() == _name)
        return *modifier;
  }
  solAssert(false, "IeleCompiler: Function modifier not found.");
}

bool IeleCompiler::isMostDerived(const FunctionDefinition *d) const {
  solAssert(!CompilingContractInheritanceHierarchy.empty(), "IeleCompiler: current contract not set.");
  for (const ContractDefinition *contract : CompilingContractInheritanceHierarchy) {
    if (d->isConstructor()) {
      return d == contract->constructor();
    }
    for (const FunctionDefinition *decl : contract->definedFunctions()) {
      if (d->name() == decl->name() && !decl->isConstructor() && FunctionType(*decl).hasEqualArgumentTypes(FunctionType(*d))) {
        return d == decl;
      }
    }
  }
  solAssert(false, "Function definition not found.");
  return false; // not reached
}

bool IeleCompiler::isMostDerived(const VariableDeclaration *d) const {
  solAssert(!CompilingContractInheritanceHierarchy.empty(), "IeleCompiler: current contract not set.");
  for (const ContractDefinition *contract : CompilingContractInheritanceHierarchy) {
    for (const VariableDeclaration *decl : contract->stateVariables()) {
      if (d->name() == decl->name()) {
        return d == decl;
      }
    }
  }
  solAssert(false, "Function definition not found.");
  return false; // not reached
}

const ContractDefinition *IeleCompiler::contractFor(const Declaration *d) const {
  solAssert(!CompilingContractInheritanceHierarchy.empty(), "IeleCompiler: current contract not set.");
  for (const ContractDefinition *contract : CompilingContractInheritanceHierarchy) {
    for (const VariableDeclaration *decl : contract->stateVariables()) {
      if (d == decl) {
        return contract;
      }
    }
    for (const FunctionDefinition *decl : contract->definedFunctions()) {
      if (d == decl) {
        return contract;
      }
    }
  }
  solAssert(false, "Declaration not found.");
  return nullptr; //not reached
}

const FunctionDefinition *IeleCompiler::superFunction(const FunctionDefinition &function, const ContractDefinition &contract) {
  solAssert(!CompilingContractInheritanceHierarchy.empty(), "IeleCompiler: current contract not set.");

  auto it = find(CompilingContractInheritanceHierarchy.begin(), CompilingContractInheritanceHierarchy.end(), &contract);
  solAssert(it != CompilingContractInheritanceHierarchy.end(), "Base not found in inheritance hierarchy.");
  it++;

  for (; it != CompilingContractInheritanceHierarchy.end(); it++) {
    const ContractDefinition *contract = *it;
    for (const FunctionDefinition *decl : contract->definedFunctions()) {
      if (function.name() == decl->name() && !decl->isConstructor() && FunctionType(*decl).hasEqualArgumentTypes(FunctionType(function))) {
        return decl;
      }
    }
  }
  solAssert(false, "Function definition not found.");
  return nullptr; // not reached
}

void IeleCompiler::compileContract(
    const ContractDefinition &contract,
    const std::map<const ContractDefinition *, const iele::IeleContract *> &contracts) {

  // Create IeleContract.
  CompilingContract = iele::IeleContract::Create(&Context, contract.name());

  // Visit state variables.
  bigint NextStorageAddress(1);
  std::vector<ContractDefinition const*> bases = contract.annotation().linearizedBaseContracts;
  // Store the current contract
  CompilingContractInheritanceHierarchy = bases;
  bool most_derived = true;

  for (const ContractDefinition *base : bases) {
    CompilingContractASTNode = base;
    for (const VariableDeclaration *stateVariable : base->stateVariables()) {
      std::string VariableName = getIeleNameForStateVariable(stateVariable);
      iele::IeleGlobalVariable *GV =
        iele::IeleGlobalVariable::Create(&Context, VariableName,
                                         CompilingContract);
      GV->setStorageAddress(iele::IeleIntConstant::Create(&Context,
                                                          NextStorageAddress++));

      if (stateVariable->isPublic()) {
        iele::IeleFunction::Create(&Context, true, getIeleNameForStateVariable(stateVariable) + "()",
                                   CompilingContract);
      }
    }
  
    if (base->constructor()) {
      if (most_derived) {
        // Then add the constructor to the symbol table.
        iele::IeleFunction::CreateInit(&Context, CompilingContract);
      } else {
        solAssert(base->constructor()->parameters().empty(), "not implemented yet: base constructor parameters.");
        iele::IeleFunction::Create(&Context, false, base->name() + ".init", CompilingContract);
      }
    }
    // Add the rest of the functions.
    for (const FunctionDefinition *function : base->definedFunctions()) {
      if (function->isConstructor() || function->isFallback() || !function->isImplemented())
        continue;
      std::string FunctionName = getIeleNameForFunction(*function);
      iele::IeleFunction::Create(&Context, function->isPublic(),
                                 FunctionName, CompilingContract);
    }
    most_derived = false;
  }
  // Add all functions to the contract's symbol table.
  if (const FunctionDefinition *fallback = contract.fallbackFunction()) {
    // First add the fallback to the symbol table, since it is not added by the
    // previous loop.
    iele::IeleFunction::CreateDeposit(&Context,
                                      fallback->isPublic(),
                                      CompilingContract);
  }

  CompilingContractASTNode = &contract;

  // Visit fallback.
  if (const FunctionDefinition *fallback = contract.fallbackFunction())
    fallback->accept(*this);

  for (auto it = bases.rbegin(); it != bases.rend(); it++) {
    const ContractDefinition *base = *it;
    CompilingContractASTNode = base;
    if (base == &contract) {
      continue;
    }
    if (const FunctionDefinition *constructor = base->constructor()) {
      constructor->accept(*this);
    } else {
      CompilingFunction =
        iele::IeleFunction::Create(&Context, false, base->name() + ".init", CompilingContract);
      CompilingFunctionStatus = iele::IeleLocalVariable::Create(&Context, "status", CompilingFunction);
      CompilingBlock =
        iele::IeleBlock::Create(&Context, "entry", CompilingFunction);
      appendStateVariableInitialization(base);
      iele::IeleInstruction::CreateRetVoid(CompilingBlock);
      CompilingBlock = nullptr;
      CompilingFunction = nullptr;
    }
  }

  // Visit constructor. If it doesn't exist create an empty @init function as
  // constructor.
  if (const FunctionDefinition *constructor = contract.constructor())
    constructor->accept(*this);
  else {
    CompilingFunction =
      iele::IeleFunction::CreateInit(&Context, CompilingContract);
    CompilingFunctionStatus = iele::IeleLocalVariable::Create(&Context, "status", CompilingFunction);
    CompilingBlock =
      iele::IeleBlock::Create(&Context, "entry", CompilingFunction);
    appendStateVariableInitialization(&contract);
    iele::IeleInstruction::CreateRetVoid(CompilingBlock);
    CompilingBlock = nullptr;
    CompilingFunction = nullptr;
  }

  // Visit functions.
  for (const ContractDefinition *base : bases) {
    CompilingContractASTNode = base;
    for (const FunctionDefinition *function : base->definedFunctions()) {
      if (function->isConstructor() || function->isFallback() || !function->isImplemented())
        continue;
      function->accept(*this);
    }
    for (const VariableDeclaration *stateVariable : base->stateVariables()) {
      if (!stateVariable->isPublic()) {
        continue;
      }
      appendAccessorFunction(stateVariable);
    }
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

void IeleCompiler::appendAccessorFunction(const VariableDeclaration *stateVariable) {
  std::string name = getIeleNameForStateVariable(stateVariable);

   // Lookup function in the contract's symbol table.
  iele::IeleValueSymbolTable *ST = CompilingContract->getIeleValueSymbolTable();
  solAssert(ST,
            "IeleCompiler: failed to access compiling contract's symbol table.");
  CompilingFunction = llvm::dyn_cast<iele::IeleFunction>(ST->lookup(name + "()"));
  solAssert(CompilingFunction,
            "IeleCompiler: failed to find function in compiling contract's"
            " symbol table");

  // Create the entry block.
  CompilingBlock =
    iele::IeleBlock::Create(&Context, "entry", CompilingFunction);

  appendPayableCheck();

  iele::IeleGlobalVariable *GV = llvm::dyn_cast<iele::IeleGlobalVariable>(ST->lookup(name));
  iele::IeleLocalVariable *LoadedValue =
    iele::IeleLocalVariable::Create(&Context, name + ".val",
                                    CompilingFunction);
  iele::IeleInstruction::CreateSLoad(LoadedValue, GV, CompilingBlock);

  llvm::SmallVector<iele::IeleValue *, 1> Returns;
  Returns.push_back(LoadedValue);

  iele::IeleInstruction::CreateRet(Returns, CompilingBlock);

  appendRevertBlocks();
  CompilingBlock = nullptr;
  CompilingFunction = nullptr;
}

bool IeleCompiler::visit(const FunctionDefinition &function) {
  std::string name = getIeleNameForFunction(function);

  // Lookup function in the contract's symbol table.
  iele::IeleValueSymbolTable *ST = CompilingContract->getIeleValueSymbolTable();
  solAssert(ST,
            "IeleCompiler: failed to access compiling contract's symbol table.");
  CompilingFunction = llvm::dyn_cast<iele::IeleFunction>(ST->lookup(name));
  solAssert(CompilingFunction,
            "IeleCompiler: failed to find function in compiling contract's"
            " symbol table");
  CompilingFunctionASTNode = &function;
  unsigned NumOfModifiers = CompilingFunctionASTNode->modifiers().size();

  // Visit formal arguments.
  for (const ASTPointer<const VariableDeclaration> &arg : function.parameters()) {
    std::string genName = arg->name() + getNextVarSuffix();
    iele::IeleArgument::Create(&Context, genName, CompilingFunction);
    // No need to keep track of the mapping for omitted args, since they will never be referenced.
    if (!(arg->name() == ""))
       VarNameMap[NumOfModifiers][arg->name()] = genName;
  }

  // We store the return params names, which we'll use when generating a default `ret`
  llvm::SmallVector<std::string, 4> ReturnParameterNames;

  // Visit formal return parameters.
  for (const ASTPointer<const VariableDeclaration> &ret : function.returnParameters()) {
    std::string genName = ret->name() + getNextVarSuffix();
    ReturnParameterNames.push_back(genName);
    iele::IeleLocalVariable::Create(&Context, genName, CompilingFunction);
    // No need to keep track of the mapping for omitted return params, since they will never be referenced.
    if (!(ret->name() == ""))
      VarNameMap[NumOfModifiers][ret->name()] = genName; 
  }

  // Visit local variables.
  for (const VariableDeclaration *local: function.localVariables()) {
    std::string genName = local->name() + getNextVarSuffix();
    VarNameMap[NumOfModifiers][local->name()] = genName; 
    iele::IeleLocalVariable::Create(&Context, genName, CompilingFunction);
  }

  CompilingFunctionStatus = iele::IeleLocalVariable::Create(&Context, "status", CompilingFunction);

  // Create the entry block.
  CompilingBlock =
    iele::IeleBlock::Create(&Context, "entry", CompilingFunction);

  if (!function.isPayable()) {
    appendPayableCheck();
  }

  // If the function is a constructor, visit state variables and add
  // initialization code.
  if (function.isConstructor())
    appendStateVariableInitialization(CompilingContractASTNode);

  // Visit function body (inc modifiers). 
  CompilingFunctionASTNode = &function;
  ModifierDepth = -1;
  appendModifierOrFunctionCode();

  // Add a ret if the last block doesn't end with a ret instruction.
  if (!CompilingBlock->endsWithRet()) {
    if (function.returnParameters().size() == 0) { // add a ret void
      iele::IeleInstruction::CreateRetVoid(CompilingBlock);
    } else { // return declared parameters
        llvm::SmallVector<iele::IeleValue *, 4> Returns;

        // Find Symbol Table for this function
        iele::IeleValueSymbolTable *ST =
          CompilingFunction->getIeleValueSymbolTable();
        solAssert(ST,
                  "IeleCompiler: failed to access compiling function's symbol "
                  "table.");

        // Prepare arguments for the `ret` instruction by fetching the param names
        for (const std::string paramName : ReturnParameterNames) {
          iele::IeleValue *param = ST->lookup(paramName);
          solAssert(param, "IeleCompiler: couldn't find parameter name in symbol table.");
          Returns.push_back(param);
        }

        // Create `ret` instruction
        iele::IeleInstruction::CreateRet(Returns, CompilingBlock);
    }
  }

  // Append the exception blocks if needed.
  appendRevertBlocks();

  CompilingBlock = nullptr;
  CompilingFunction = nullptr;
  return false;
}

void IeleCompiler::appendRevertBlocks(void) {
  if (RevertBlock) {
    RevertBlock->insertInto(CompilingFunction);
    RevertBlock = nullptr;
  }
  if (RevertStatusBlock) {
    RevertStatusBlock->insertInto(CompilingFunction);
    RevertStatusBlock = nullptr;
  }
  if (AssertFailBlock) {
    AssertFailBlock->insertInto(CompilingFunction);
    AssertFailBlock = nullptr;
  }
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
        RHSValue = appendTypeConversion(RHSValue, *RHSType, *LHSType);
        solAssert(!shouldCopyStorageToStorage(LHSValue, RHSType),
                  "IeleCompiler: found copy storage to storage in a variable "
                  "declaration statement");
        solAssert(!shouldCopyMemoryToStorage(*LHSType, *RHSType),
                  "IeleCompiler: found copy memory to storage in a variable "
                  "declaration statement");

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

  iele::IeleLocalVariable *ResultValue =
    appendConditional(ConditionValue, 
      [&condition, this]{
        iele::IeleValue *Result = compileExpression(condition.trueExpression());
        return appendTypeConversion(Result, *condition.trueExpression().annotation().type, *condition.annotation().type);
      },
      [&condition, this]{
        iele::IeleValue *Result = compileExpression(condition.falseExpression());
        return appendTypeConversion(Result, *condition.falseExpression().annotation().type, *condition.annotation().type);
      });
  CompilingExpressionResult.push_back(ResultValue);
  return false;
}

iele::IeleLocalVariable *IeleCompiler::appendConditional(
  iele::IeleValue *ConditionValue,
  const std::function<iele::IeleValue *(void)> &TrueExpression,
  const std::function<iele::IeleValue *(void)> &FalseExpression) {
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
  iele::IeleValue *FalseValue = FalseExpression();
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
  iele::IeleValue *TrueValue = TrueExpression();
  iele::IeleInstruction::CreateAssign(
    ResultValue, TrueValue, CompilingBlock);

  // Add the join block at the end of the function and compilation continues in
  // the join block.
  JoinBlock->insertInto(CompilingFunction);
  CompilingBlock = JoinBlock;
  return ResultValue;
}

bool IeleCompiler::visit(const Assignment &assignment) {
  Token::Value op = assignment.assignmentOperator();
  const Expression &LHS = assignment.leftHandSide();
  const Expression &RHS = assignment.rightHandSide();
  solAssert(LHS.annotation().type->category() != Type::Category::Tuple,
            "not implemented yet");

  // Check if we need to do a memory or storage copy.
  TypePointer LHSType = LHS.annotation().type;
  TypePointer RHSType = RHS.annotation().type;

  // Visit RHS.
  iele::IeleValue *RHSValue = compileExpression(RHS);
  solAssert(RHSValue, "IeleCompiler: Failed to compile RHS of assignment");

  RHSValue = appendTypeConversion(RHSValue, *RHSType, *LHSType);

  // Visit LHS.
  iele::IeleValue *LHSValue = compileLValue(LHS);
  solAssert(LHSValue, "IeleCompiler: Failed to compile LHS of assignment");

  if (shouldCopyStorageToStorage(LHSValue, RHSType))
    RHSValue = appendIeleRuntimeStorageToStorageCopy(RHSValue);

  // Check for compound assignment.
  if (op != Token::Assign) {
    Token::Value binOp = Token::AssignmentToBinaryOp(op);
    iele::IeleValue *LHSDeref = appendLValueDereference(LHSValue);
    RHSValue = appendBinaryOperator(binOp, LHSDeref, RHSValue, assignment.annotation().type);
  }

  // Generate assignment code.
  appendLValueAssign(LHSValue, RHSValue);

  // The result of the expression is the RHS.
  CompilingExpressionResult.push_back(RHSValue);
  return false;
}

bool IeleCompiler::visit(const TupleExpression &tuple) {

  llvm::SmallVector<iele::IeleValue *, 4> Results;

  for (unsigned i = 0; i < tuple.components().size(); i++)
    Results.push_back(compileExpression(*tuple.components()[i]));

  CompilingExpressionResult.insert(
    CompilingExpressionResult.end(), Results.begin(), Results.end());

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
    // Generate code for the inc/dec operation.
    iele::IeleIntConstant *One = iele::IeleIntConstant::getOne(&Context);
    iele::IeleLocalVariable *After =
      appendBinaryOperator(BinOperator, Before, One, unaryOperation.annotation().type);
    iele::IeleLocalVariable *Result = nullptr;
    if (!unaryOperation.isPrefixOperation()) {
      // Save the initial subexpression value in case of a postfix oparation.
      Result =
        iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
      iele::IeleInstruction::CreateAssign(Result, Before, CompilingBlock);
    } else {
      // In case of a prefix operation, the result is the value after the
      // inc/dec operation.
      Result = After;
    }
    // Generate assignment code.
    appendLValueAssign(SubExprValue, After);
    // Return result.
    CompilingExpressionResult.push_back(Result);
    break;
  }
  case Token::BitNot: { // ~
    // Visit subexpression.
    iele::IeleValue *SubExprValue =
      compileExpression(unaryOperation.subExpression());
    solAssert(SubExprValue, "IeleCompiler: Failed to compile operand.");

    // Compile as a bitwise negation.
    iele::IeleLocalVariable *Result =
      iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
    iele::IeleInstruction::CreateNot(Result, SubExprValue, CompilingBlock);
    CompilingExpressionResult.push_back(Result);
    break;
  }
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
  if (binaryOperation.getOperator() == Token::Or || binaryOperation.getOperator() == Token::And) {
    iele::IeleValue *Result =
      appendBooleanOperator(binaryOperation.getOperator(),
                            binaryOperation.leftExpression(),
                            binaryOperation.rightExpression());
    CompilingExpressionResult.push_back(Result);
    return false;
  }
  // Visit operands.
  iele::IeleValue *LeftOperandValue = 
    compileExpression(binaryOperation.leftExpression());
  solAssert(LeftOperandValue, "IeleCompiler: Failed to compile left operand.");
  iele::IeleValue *RightOperandValue = 
    compileExpression(binaryOperation.rightExpression());
  solAssert(RightOperandValue, "IeleCompiler: Failed to compile right operand.");

  const TypePointer &commonType = binaryOperation.annotation().commonType;
  if (commonType->category() == Type::Category::RationalNumber) {
    iele::IeleIntConstant *LiteralValue =
      iele::IeleIntConstant::Create(
        &Context,
        commonType->literalValue(nullptr));
    CompilingExpressionResult.push_back(LiteralValue);
    return false;
  }

  LeftOperandValue = appendTypeConversion(LeftOperandValue,
    *binaryOperation.leftExpression().annotation().type,
    *commonType);
  if (Token::isShiftOp(binaryOperation.getOperator())) {
    RightOperandValue = appendTypeConversion(RightOperandValue,
      *binaryOperation.rightExpression().annotation().type,
      *commonType);
  }
  // Append the IELE code for the binary operator.
  iele::IeleValue *Result =
    appendBinaryOperator(binaryOperation.getOperator(),
                         LeftOperandValue, RightOperandValue,
                         binaryOperation.annotation().type);

  CompilingExpressionResult.push_back(Result);
  return false;
}

bool IeleCompiler::visit(const FunctionCall &functionCall) {
  // Not supported yet cases.
  if (functionCall.annotation().kind == FunctionCallKind::TypeConversion) {
    solAssert(functionCall.arguments().size() == 1, "");
    solAssert(functionCall.names().empty(), "");
    iele::IeleValue *ArgumentValue =
      compileExpression(*functionCall.arguments().front().get());
    solAssert(ArgumentValue,
           "IeleCompiler: Failed to compile conversion target.");
    iele::IeleValue *ResultValue = appendTypeConversion(ArgumentValue,
      *functionCall.arguments().front()->annotation().type,
      *functionCall.annotation().type);
    CompilingExpressionResult.push_back(ResultValue);
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
                                      bigint(arguments.size())));

    // Visit arguments and initialize struct fields.
    for (unsigned i = 0; i < arguments.size(); ++i) {
      iele::IeleValue *InitValue = compileExpression(*arguments[i]);
      InitValue = appendTypeConversion(InitValue, *arguments[i]->annotation().type, *functionType->parameterTypes()[i]);
      iele::IeleValue *AddressValue =
        appendIeleRuntimeMemoryAddress(
            StructValue,
            iele::IeleIntConstant::Create(&Context, bigint(i)));
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
    TransferValue = appendTypeConversion(TransferValue,
      *arguments.front()->annotation().type,
      *function.parameterTypes().front());
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
    iele::IeleInstruction::CreateAccountCall(false, StatusValue, EmptyLValues, Deposit,
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
    ConditionValue = appendTypeConversion(ConditionValue,
      *arguments.front()->annotation().type,
      *function.parameterTypes().front());
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
    Op1 = appendTypeConversion(Op1, *arguments[0]->annotation().type, SInt);
    solAssert(Op1,
           "IeleCompiler: Failed to compile operand 1 of addmod.");
    iele::IeleValue *Op2 = compileExpression(*arguments[1].get());
    Op2 = appendTypeConversion(Op2, *arguments[1]->annotation().type, SInt);
    solAssert(Op2,
           "IeleCompiler: Failed to compile operand 2 of addmod.");
    iele::IeleValue *Modulus = compileExpression(*arguments[2].get());
    Modulus = appendTypeConversion(Modulus, *arguments[2]->annotation().type, SInt);
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
    Op1 = appendTypeConversion(Op1, *arguments[0]->annotation().type, SInt);
    solAssert(Op1,
           "IeleCompiler: Failed to compile operand 1 of addmod.");
    iele::IeleValue *Op2 = compileExpression(*arguments[1].get());
    Op2 = appendTypeConversion(Op2, *arguments[1]->annotation().type, SInt);
    solAssert(Op2,
           "IeleCompiler: Failed to compile operand 2 of addmod.");
    iele::IeleValue *Modulus = compileExpression(*arguments[2].get());
    Modulus = appendTypeConversion(Modulus, *arguments[2]->annotation().type, SInt);
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
    break;
  }
  case FunctionType::Kind::Internal: {
    // Visit arguments.
    llvm::SmallVector<iele::IeleValue *, 4> Arguments;
    for (unsigned i = 0; i < arguments.size(); ++i) {
      iele::IeleValue *ArgValue = compileExpression(*arguments[i]);
      solAssert(ArgValue,
                "IeleCompiler: Failed to compile internal function call "
                "argument");
      // Check if we need to do a memory to/from storage copy.
      TypePointer ArgType = arguments[i]->annotation().type;
      TypePointer ParamType = function.parameterTypes()[i];
      ArgValue = appendTypeConversion(ArgValue, *ArgType, *ParamType);
      Arguments.push_back(ArgValue);
    }

    // Visit callee.
    iele::IeleGlobalValue *CalleeValue =
      llvm::dyn_cast<iele::IeleGlobalValue>(
          compileExpression(functionCall.expression()));
    solAssert(CalleeValue,
              "IeleCompiler: Failed to compile callee of internal function "
              "call");

    // Prepare registers for return values.
    llvm::SmallVector<iele::IeleLocalVariable *, 4> Returns;
    for (unsigned i = 0; i < function.returnParameterTypes().size(); ++i)
      Returns.push_back(
        iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction));

    // Generate call and return values.
    iele::IeleInstruction::CreateInternalCall(Returns, CalleeValue, Arguments,
                                              CompilingBlock);
    CompilingExpressionResult.insert(
        CompilingExpressionResult.end(), Returns.begin(), Returns.end());
    break;
  }
  case FunctionType::Kind::Selfdestruct: {
    // Visit argument (target of the Selfdestruct)
    iele::IeleValue *TargetAddress =
      compileExpression(*arguments.front().get());
    TargetAddress = appendTypeConversion(TargetAddress,
      *arguments.front()->annotation().type,
      *function.parameterTypes().front());
    solAssert(TargetAddress,
              "IeleCompiler: Failed to compile Selfdestruct target.");

    // Make IELE instruction
    iele::IeleInstruction::CreateSelfdestruct(TargetAddress, CompilingBlock);
    break;
  }
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

  if (auto type = dynamic_cast<const ContractType *>(memberAccess.expression().annotation().type.get())) {
 
    if (type->isSuper()) {
      iele::IeleValueSymbolTable *ST = CompilingContract->getIeleValueSymbolTable();
      solAssert(ST,
                "IeleCompiler: failed to access compiling contract's symbol table.");

      const FunctionDefinition *super = superFunction(dynamic_cast<const FunctionDefinition &>(*memberAccess.annotation().referencedDeclaration), type->contractDefinition());
      std::string superName = getIeleNameForFunction(*super);
      iele::IeleValue *Result = ST->lookup(superName);
      solAssert(Result,
                "IeleCompiler: failed to find function in compiling contract's"
                " symbol table");
      CompilingExpressionResult.push_back(Result);
      return false;
    } else {
      memberAccess.expression().accept(*this);
      if (const Declaration *declaration = memberAccess.annotation().referencedDeclaration) {
        std::string name;
        // don't call getIeleNameFor here because this is part of an external call and therefore is only able to
        // see the most-derived function
        if (const auto *variable = dynamic_cast<const VariableDeclaration *>(declaration)) {
          name = variable->name() + "()";
        } else if (const auto *function = dynamic_cast<const FunctionDefinition *>(declaration)) {
          name = function->externalSignature();
        } else {
          solAssert(false, "Contract member is neither variable nor function.");
        }
        iele::IeleValue *Result = iele::IeleFunction::Create(&Context, true, name);
        CompilingExpressionResult.push_back(Result);
        return false;
      }
    }
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
  case Type::Category::Contract:
  case Type::Category::Integer: {
    solAssert(!(std::set<std::string>{"call", "callcode", "delegatecall"}).count(member),
              "IeleCompiler: member not supported in IELE");
    if (member == "transfer" || member == "send") {
      ExprValue = appendTypeConversion(ExprValue,
        *memberAccess.expression().annotation().type,
        Address);
      solAssert(ExprValue, "IeleCompiler: Failed to compile address");
      CompilingExpressionResult.push_back(ExprValue);
    } else if (member == "balance") {
      ExprValue = appendTypeConversion(ExprValue,
        *memberAccess.expression().annotation().type,
        Address);
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
          bigint(getStructMemberIndex(type, member)));
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
  case Type::Category::FixedBytes: {
    const FixedBytesType &type = dynamic_cast<const FixedBytesType &>(baseType);
    if (member != "length")
      solAssert(false, "IeleCompiler: invalid member for bytesN value");
    iele::IeleValue *LengthValue =
      iele::IeleIntConstant::Create(
        &Context,
        bigint(type.numBytes()));
    CompilingExpressionResult.push_back(LengthValue);
    break;
  }
  case Type::Category::Array:
  case Type::Category::Enum:
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
    IndexValue = appendTypeConversion(IndexValue, *indexAccess.indexExpression()->annotation().type, IntegerType(256));
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
  case Type::Category::FixedBytes: {
    const FixedBytesType &type = dynamic_cast<const FixedBytesType &>(baseType);

    solAssert(indexAccess.indexExpression(),
              "IeleCompiler: Index expression expected.");

     // Visit index expression.
    iele::IeleValue *IndexValue =
      compileExpression(*indexAccess.indexExpression());
    IndexValue = appendTypeConversion(IndexValue, *indexAccess.indexExpression()->annotation().type, IntegerType(256));
    solAssert(IndexValue,
              "IeleCompiler: failed to compile index expression for index "
              "access.");

    iele::IeleLocalVariable *OutOfRangeValue =
      iele::IeleLocalVariable::Create(&Context, "index.out.of.range",
                                      CompilingFunction);
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::CmpGe, OutOfRangeValue, IndexValue, 
      iele::IeleIntConstant::getZero(&Context),
      CompilingBlock);
    appendRevert(OutOfRangeValue);

    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::CmpLt, OutOfRangeValue, IndexValue,
      iele::IeleIntConstant::Create(&Context, bigint(type.numBytes())),
      CompilingBlock);
    appendRevert(OutOfRangeValue);

    iele::IeleLocalVariable *ShiftValue =
      iele::IeleLocalVariable::Create(&Context, "tmp",
                                      CompilingFunction);
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Sub, ShiftValue, IndexValue,
      iele::IeleIntConstant::Create(&Context, bigint(type.numBytes())),
      CompilingBlock);
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Shift, ShiftValue, ExprValue, ShiftValue, CompilingBlock);
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::And, ShiftValue, ShiftValue,
      iele::IeleIntConstant::Create(&Context, bigint(255)),
      CompilingBlock);
    CompilingExpressionResult.push_back(ShiftValue);
    break;
  }
  case Type::Category::Mapping:
  case Type::Category::TypeType:
    solAssert(false, "not implemented yet");
  default:
    solAssert(false, "IeleCompiler: Index access to unknown type.");
  }

  return false;
}

void IeleCompiler::endVisit(const Identifier &identifier) {
  // Get the corrent name for the identifier.
  std::string name = identifier.name();
  const Declaration *declaration =
    identifier.annotation().referencedDeclaration;
  if (const FunctionDefinition *functionDef =
        dynamic_cast<const FunctionDefinition *>(declaration))
    name = getIeleNameForFunction(*functionDef); 

  // Check if identifier is a reserved identifier.
  if (const MagicVariableDeclaration *magicVar =
         dynamic_cast<const MagicVariableDeclaration *>(declaration)) {
    switch (magicVar->type()->category()) {
    case Type::Category::Contract: {
      llvm::SmallVector<iele::IeleValue *, 0> EmptyArguments;
      iele::IeleLocalVariable *This =
        iele::IeleLocalVariable::Create(&Context, "this", CompilingFunction);
      iele::IeleInstruction::CreateIntrinsicCall(iele::IeleInstruction::Address, This, EmptyArguments, CompilingBlock);
      CompilingExpressionResult.push_back(This);
      return;
    }
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
          type->literalValue(&literal));
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

void IeleCompiler::appendPayableCheck() {
  llvm::SmallVector<iele::IeleValue *, 0> EmptyArguments;

  iele::IeleLocalVariable *CallvalueValue =
    iele::IeleLocalVariable::Create(&Context, "callvalue", CompilingFunction);
  iele::IeleInstruction::CreateIntrinsicCall(
    iele::IeleInstruction::Callvalue, CallvalueValue, EmptyArguments,
    CompilingBlock);

  appendRevert(CallvalueValue);
}

void IeleCompiler::appendRevert(iele::IeleValue *Condition, iele::IeleValue *Status) {
  iele::IeleBlock *Block;
  if (Status) {
    if (!RevertStatusBlock) {
      // Create the status-specific revert block if it's not already created.
      RevertStatusBlock = iele::IeleBlock::Create(&Context, "throw.status");
      iele::IeleInstruction::CreateRevert(
          CompilingFunctionStatus, RevertStatusBlock);
    }
    Block = RevertStatusBlock;
    iele::IeleInstruction::CreateAssign(
        CompilingFunctionStatus, Status, CompilingBlock);
  } else {
    // Create the revert block if it's not already created.
    if (!RevertBlock) {
      RevertBlock = iele::IeleBlock::Create(&Context, "throw");
      iele::IeleInstruction::CreateRevert(
          iele::IeleIntConstant::getMinusOne(&Context), RevertBlock);
    }
    Block = RevertBlock;
  }

  if (Condition)
    connectWithConditionalJump(Condition, CompilingBlock, Block);
  else
    connectWithUnconditionalJump(CompilingBlock, Block);
}

void IeleCompiler::appendInvalid(iele::IeleValue *Condition) {
  // Create the assert-fail block if it's not already created.
  if (!AssertFailBlock) {
    AssertFailBlock = iele::IeleBlock::Create(&Context, "invalid");
  }

  if (Condition)
    connectWithConditionalJump(Condition, CompilingBlock, AssertFailBlock);
  else
    connectWithUnconditionalJump(CompilingBlock, AssertFailBlock);
}

void IeleCompiler::appendStateVariableInitialization(const ContractDefinition *contract) {
  bool found = false;
  for (const ContractDefinition *def : CompilingContractInheritanceHierarchy) {
    if (found) {
      llvm::SmallVector<iele::IeleLocalVariable *, 4> Returns;
      llvm::SmallVector<iele::IeleValue *, 4> Arguments;
      iele::IeleValueSymbolTable *ST = CompilingContract->getIeleValueSymbolTable();
      solAssert(ST,
                "IeleCompiler: failed to access compiling function's symbol "
                "table.");
      iele::IeleValue *ConstructorValue = ST->lookup(def->name() + ".init");
      solAssert(ConstructorValue, "IeleCompiler: failed to find constructor for contract " + def->name());
      iele::IeleGlobalValue *CalleeValue =
        llvm::dyn_cast<iele::IeleGlobalValue>(ConstructorValue);
      iele::IeleInstruction::CreateInternalCall(Returns, CalleeValue, Arguments, CompilingBlock);
      break;
    }
    if (def == contract) {
      found = true;
    }
  }
  for (const VariableDeclaration *stateVariable :
         contract->stateVariables()) {
    // Visit initialization value if it exists.
    iele::IeleValue *InitValue = nullptr;
    TypePointer type = stateVariable->annotation().type;
    if (stateVariable->value())
      InitValue = appendTypeConversion(compileExpression(*stateVariable->value()), *stateVariable->value()->annotation().type, *type);
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
              bigint(structType.structDefinition().members().size()));
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
    iele::IeleValue *LHSValue = ST->lookup(getIeleNameForStateVariable(stateVariable));
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

iele::IeleLocalVariable *IeleCompiler::appendBooleanOperator(
    Token::Value Opcode,
    const Expression &LeftOperand,
    const Expression &RightOperand) {
  solAssert(Opcode == Token::Or || Opcode == Token::And, "IeleCompiler: invalid boolean operator");

  iele::IeleValue *LeftOperandValue = 
    compileExpression(LeftOperand);
  solAssert(LeftOperandValue, "IeleCompiler: Failed to compile left operand.");
  if (Opcode == Token::Or) {

    return appendConditional(
      LeftOperandValue, 
      [this]{return iele::IeleIntConstant::getOne(&Context); },
      [&RightOperand, this]{return compileExpression(RightOperand);});
  } else {
    return appendConditional(
      LeftOperandValue, 
      [&RightOperand, this]{return compileExpression(RightOperand);},
      [this]{return iele::IeleIntConstant::getZero(&Context); });
  }
}

iele::IeleLocalVariable *IeleCompiler::appendBinaryOperator(
    Token::Value Opcode,
    iele::IeleValue *LeftOperand,
    iele::IeleValue *RightOperand,
    TypePointer ResultType) {
  // Find corresponding IELE binary opcode.
  iele::IeleInstruction::IeleOps BinOpcode;

  bool fixed = false, issigned = false;
  int nbytes;
  switch (ResultType->category()) {
  case Type::Category::Integer: {
    const IntegerType *type = dynamic_cast<const IntegerType *>(ResultType.get());
    fixed = type->numBits() != 256;
    nbytes = type->numBits() / 8;
    issigned = type->isSigned();
    break;
  }
  case Type::Category::FixedBytes: {
    const FixedBytesType *type = dynamic_cast<const FixedBytesType *>(ResultType.get());
    fixed = true;
    nbytes = type->numBytes();
    break;
  }
  default: break;
  }

  switch (Opcode) {
  case Token::Add:                BinOpcode = iele::IeleInstruction::Add; break;
  case Token::Sub:                BinOpcode = iele::IeleInstruction::Sub; break;
  case Token::Mul: {
    if (fixed) {
      // Create the instruction.
      iele::IeleValue *ModulusValue =
        iele::IeleIntConstant::Create(&Context, bigint(1) << (nbytes * 8));
      iele::IeleLocalVariable *Result =
        iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
      iele::IeleInstruction::CreateTernOp(
        iele::IeleInstruction::MulMod, Result, LeftOperand, RightOperand,
        ModulusValue, CompilingBlock);


      iele::IeleValue *NBytesValue =
        iele::IeleIntConstant::Create(&Context, bigint(nbytes-1));
      if (issigned) {
        iele::IeleInstruction::CreateBinOp(
          iele::IeleInstruction::SExt, Result,NBytesValue, Result,
          CompilingBlock);
      }
      return Result;
    }
    BinOpcode = iele::IeleInstruction::Mul; break;
  }
  case Token::Div:                BinOpcode = iele::IeleInstruction::Div; break;
  case Token::Mod:                BinOpcode = iele::IeleInstruction::Mod; break;
  case Token::Exp: {
    if (fixed) {
      // Create the instruction.
      iele::IeleValue *ModulusValue =
        iele::IeleIntConstant::Create(&Context, bigint(1) << (nbytes * 8));
      iele::IeleLocalVariable *Result =
        iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
      iele::IeleInstruction::CreateTernOp(
        iele::IeleInstruction::ExpMod, Result, LeftOperand, RightOperand,
        ModulusValue, CompilingBlock);

      iele::IeleValue *NBytesValue =
        iele::IeleIntConstant::Create(&Context, bigint(nbytes-1));
      if (issigned) {
        iele::IeleInstruction::CreateBinOp(
          iele::IeleInstruction::SExt, Result, NBytesValue, Result,
          CompilingBlock);
      }
      return Result;
    }
    BinOpcode = iele::IeleInstruction::Exp; break;
  }
  case Token::Equal:              BinOpcode = iele::IeleInstruction::CmpEq; break;
  case Token::NotEqual:           BinOpcode = iele::IeleInstruction::CmpNe; break;
  case Token::GreaterThanOrEqual: BinOpcode = iele::IeleInstruction::CmpGe; break;
  case Token::LessThanOrEqual:    BinOpcode = iele::IeleInstruction::CmpLe; break;
  case Token::GreaterThan:        BinOpcode = iele::IeleInstruction::CmpGt; break;
  case Token::LessThan:           BinOpcode = iele::IeleInstruction::CmpLt; break;
  case Token::BitAnd:             BinOpcode = iele::IeleInstruction::And; break;
  case Token::BitOr:              BinOpcode = iele::IeleInstruction::Or; break;
  case Token::BitXor:             BinOpcode = iele::IeleInstruction::Xor; break;
  case Token::SAR: {
    iele::IeleLocalVariable *ShiftValue =
      iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Sub, ShiftValue,
      iele::IeleIntConstant::getZero(&Context),
      RightOperand, CompilingBlock);
    RightOperand = ShiftValue;
    // fall through
  }
  case Token::SHL:                BinOpcode = iele::IeleInstruction::Shift; break;
  
  default:
    solAssert(false, "not implemented yet");
    solAssert(false, "IeleCompiler: Invalid binary operator");
  }

  // Create the instruction.
  iele::IeleLocalVariable *Result =
    iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
  iele::IeleInstruction::CreateBinOp(
      BinOpcode, Result, LeftOperand, RightOperand, CompilingBlock);

  if (fixed) {
    switch(Opcode) {
    case Token::Add:
    case Token::Sub:
    case Token::Div:
    case Token::Mod:
    case Token::SHL: appendMask(Result, Result, nbytes, issigned); break;
    case Token::Mul:
    case Token::Exp: break; //handled above
    case Token::Equal:
    case Token::NotEqual:
    case Token::GreaterThanOrEqual:
    case Token::LessThanOrEqual:
    case Token::GreaterThan:
    case Token::LessThan:
    case Token::BitAnd:
    case Token::BitOr:
    case Token::BitXor:
    case Token::SAR: break; // can't overflow
  
    default:
      solAssert(false, "not implemented yet");
      solAssert(false, "IeleCompiler: Invalid binary operator");
    }
  }

  return Result;
}

void IeleCompiler::appendShiftBy(iele::IeleLocalVariable *ResultValue, iele::IeleValue *Value, int shiftAmount) {
  iele::IeleIntConstant *ShiftValue =
    iele::IeleIntConstant::Create(&Context, bigint(shiftAmount));
  iele::IeleInstruction::CreateBinOp(
    iele::IeleInstruction::Shift, ResultValue, Value, ShiftValue, CompilingBlock);
}

iele::IeleValue *IeleCompiler::appendTypeConversion(iele::IeleValue *Value, const Type& SourceType, const Type& TargetType) {
  if (SourceType == TargetType) {
    return Value;
  }
  iele::IeleLocalVariable *convertedValue =
    iele::IeleLocalVariable::Create(&Context, "type.converted", CompilingFunction);
  switch (SourceType.category()) {
  case Type::Category::FixedBytes: {
    const FixedBytesType &srcType = dynamic_cast<const FixedBytesType &>(SourceType);
    if (TargetType.category() == Type::Category::Integer) {
      IntegerType const& targetType = dynamic_cast<const IntegerType &>(TargetType);
      if (targetType.numBits() < srcType.numBytes() * 8) {
        appendMask(convertedValue, Value, targetType.numBits() / 8, targetType.isSigned());
        return convertedValue;
      }
      return Value;
    } else {
      solAssert(TargetType.category() == Type::Category::FixedBytes, "Invalid type conversion requested.");
      const FixedBytesType &targetType = dynamic_cast<const FixedBytesType &>(TargetType);
      appendShiftBy(convertedValue, Value, 8 * (targetType.numBytes() - srcType.numBytes()));
      return convertedValue;
    }
  }
  case Type::Category::Enum:
  case Type::Category::FixedPoint:
  case Type::Category::Tuple:
  case Type::Category::Function: {
    solAssert(false, "not implemented yet");
    break;
  case Type::Category::Struct:
  case Type::Category::Array:
    if (shouldCopyStorageToMemory(TargetType, SourceType))
      return appendIeleRuntimeStorageToMemoryCopy(Value);
    else if (shouldCopyMemoryToStorage(TargetType, SourceType))
      return appendIeleRuntimeMemoryToStorageCopy(Value);
    return Value;
  case Type::Category::Integer:
  case Type::Category::RationalNumber:
  case Type::Category::Contract: {
    switch(TargetType.category()) {
    case Type::Category::FixedBytes: {
      solAssert(SourceType.category() == Type::Category::Integer || SourceType.category() == Type::Category::RationalNumber,
        "Invalid conversion to FixedBytesType requested.");
      if (auto srcType = dynamic_cast<const IntegerType *>(&SourceType)) {
        const FixedBytesType &targetType = dynamic_cast<const FixedBytesType &>(TargetType);
        if (targetType.numBytes() * 8 < srcType->numBits()) {
          appendMask(convertedValue, Value, targetType.numBytes(), false);
          return convertedValue;
        }
      }
      return Value;
    }
    case Type::Category::Enum:
    case Type::Category::StringLiteral:
      solAssert(false, "not implemented yet");
      break;
    case Type::Category::FixedPoint:
      solUnimplemented("Not yet implemented - FixedPointType.");
      break;
    case Type::Category::Integer:
    case Type::Category::Contract: {
      IntegerType const& targetType = TargetType.category() == Type::Category::Integer
        ? dynamic_cast<const IntegerType &>(TargetType) : Address;
      if (SourceType.category() == Type::Category::RationalNumber) {
        solAssert(!dynamic_cast<const RationalNumberType &>(SourceType).isFractional(), "not implemented yet");
      }
      if (targetType.numBits() == 256) {
        if (!targetType.isSigned()) {
          if (iele::IeleIntConstant *constant = llvm::dyn_cast<iele::IeleIntConstant>(Value)) {
            if (constant->getValue() < 0) {
              appendRevert();
            }
          } else {
            iele::IeleLocalVariable *OutOfRange =
              iele::IeleLocalVariable::Create(&Context, "integer.out.of.range", CompilingFunction);
            iele::IeleInstruction::CreateBinOp(
              iele::IeleInstruction::CmpLt, OutOfRange, Value,
              iele::IeleIntConstant::getZero(&Context),
              CompilingBlock);
            appendRevert(OutOfRange);
          }
        }
        return Value;
      }
      if (iele::IeleIntConstant *constant = llvm::dyn_cast<iele::IeleIntConstant>(Value)) {
        if (constant->getValue() < (targetType.isSigned() ? bigint(1) << targetType.numBits() - 1 : bigint(1) << targetType.numBits())
          && constant->getValue() >= (targetType.isSigned() ? -(bigint(1) << targetType.numBits() - 1) : bigint(0))) {
          return Value;
        }
      }
      appendMask(convertedValue, Value, targetType.numBits() / 8, targetType.isSigned());
      return convertedValue;
    }
    default:
      solAssert(false, "Invalid type conversion requested.");
      return nullptr;
    }
  }
  case Type::Category::Bool:
    solAssert(SourceType == TargetType, "Invalid conversion for bool.");
    return Value;
  }
  default:
    solAssert(false, "Invalid type conversion requested.");
    return nullptr;
  }
}

void IeleCompiler::appendMask(iele::IeleLocalVariable *Result, iele::IeleValue *Value, int nbytes, bool issigned) {
  iele::IeleValue *NBytesValue =
    iele::IeleIntConstant::Create(&Context, bigint(nbytes));
  iele::IeleInstruction::CreateBinOp(
    iele::IeleInstruction::Twos, Result, NBytesValue, Value,
    CompilingBlock);
  if (issigned) {
    NBytesValue =
      iele::IeleIntConstant::Create(&Context, bigint(nbytes-1));
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::SExt, Result, NBytesValue, Result,
      CompilingBlock);
  }
}

bool IeleCompiler::shouldCopyStorageToStorage(iele::IeleValue *To,
                                              TypePointer From) const {
  return llvm::isa<iele::IeleGlobalVariable>(To) &&
         From->dataStoredIn(DataLocation::Storage);
}

bool IeleCompiler::shouldCopyMemoryToStorage(const Type &To,
                                             const Type &From) const {
  return To.dataStoredIn(DataLocation::Storage) &&
         From.dataStoredIn(DataLocation::Memory);
}

bool IeleCompiler::shouldCopyStorageToMemory(const Type &To,
                                             const Type &From) const {
  return To.dataStoredIn(DataLocation::Memory) &&
         From.dataStoredIn(DataLocation::Storage);
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
