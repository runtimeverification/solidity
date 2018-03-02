#include "libsolidity/codegen/IeleCompiler.h"

#include "libiele/IeleContract.h"
#include "libiele/IeleGlobalVariable.h"
#include "libiele/IeleIntConstant.h"

#include <iostream>
#include "llvm/Support/raw_ostream.h"

using namespace dev;
using namespace dev::solidity;

void IeleCompiler::compileContract(
    ContractDefinition const &contract,
    std::map<ContractDefinition const*, iele::IeleContract const*> const &contracts) {
  // Create IeleContract.
  CompilingContract = iele::IeleContract::Create(&Context, contract.name());

  // Visit state variables.
  llvm::APInt NextStorageAddress(64, 1); // here we should use a true unbound
                                         // integer datatype but using llvm's
                                         // APINt for now.
  for (const VariableDeclaration *stateVariable : contract.stateVariables()) {
    assert(!stateVariable->value().get() && "not implemented yet");
    iele::IeleGlobalVariable *GV =
      iele::IeleGlobalVariable::Create(&Context, stateVariable->name(),
                                       CompilingContract);
    assert(NextStorageAddress != 0 &&
           "IeleCompiler: Overflow: more state variables than currently "
           "supported");
    GV->setStorageAddress(iele::IeleIntConstant::Create(&Context,
                                                        NextStorageAddress++));
  }

  // Visit fallback. If it doesn't exist create an empty @deposit function as
  // fallback.
  if (const FunctionDefinition *fallback = contract.fallbackFunction())
    fallback->accept(*this);
  else {
    iele::IeleFunction *Fallback =
      iele::IeleFunction::Create(&Context, true, "deposit", CompilingContract);
    iele::IeleBlock *FallbackBlock =
      iele::IeleBlock::Create(&Context, "entry", Fallback);
    iele::IeleInstruction::CreateRetVoid(FallbackBlock);
  }

  // Visit constructor. If it doesn't exist create an empty @init function as
  // constructor.
  if (const FunctionDefinition *constructor = contract.constructor())
    constructor->accept(*this);
  else {
    iele::IeleFunction *Constructor =
      iele::IeleFunction::Create(&Context, true, "init", CompilingContract);
    iele::IeleBlock *ConstructorBlock =
      iele::IeleBlock::Create(&Context, "entry", Constructor);
    iele::IeleInstruction::CreateRetVoid(ConstructorBlock);
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

bool IeleCompiler::visit(FunctionDefinition const& function) {
  std::string FunctionName = function.name();
  if (function.isConstructor())
    FunctionName = "init";
  else if (function.isFallback())
    FunctionName = "deposit";

  CompilingFunction =
    iele::IeleFunction::Create(&Context, function.isPartOfExternalInterface(),
                               FunctionName, CompilingContract);

  // Visit formal arguments.
  for (ASTPointer<VariableDeclaration const> const& arg : function.parameters())
    iele::IeleArgument::Create(&Context, arg->name(), CompilingFunction);

  // Create the entry block.
  CompilingBlock =
    iele::IeleBlock::Create(&Context, "entry", CompilingFunction);

  // Visit function's body.
  function.body().accept(*this);

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
  // Save condition block.
  iele::IeleBlock *ConditionBlock = CompilingBlock;

  // Check if we have an if-false block. Our compilation strategy depends on
  // that.
  bool HasIfFalse = ifStatement.falseStatement() != nullptr;

  // Visit condition.
  iele::IeleValue * ConditionValue =
    compileExpression(ifStatement.condition());
  assert(ConditionValue && "IeleCompiler: Failed to compile if condition.");

  // If we don't have an if-false block, then we invert the condition.
  if (!HasIfFalse) {
    iele::IeleLocalVariable *InvConditionValue =
      iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
    iele::IeleInstruction::CreateIsZero(InvConditionValue, ConditionValue,
                                        CompilingBlock);
    ConditionValue = InvConditionValue;
  }

  // If we have an if-false block, then the join block is the if-true block,
  // else it is a new block.
  iele::IeleBlock *JoinBlock =
    iele::IeleBlock::Create(&Context, HasIfFalse ? "if.true" : "if.end",
                            CompilingFunction);

  // Connect the condition block with a conditional jump to the join block.
  connectWithConditionalJump(ConditionValue, ConditionBlock, JoinBlock);

  // Append the code for the if-false block if we have one, else append the code
  // for the if-true block.
  if (HasIfFalse)
    ifStatement.falseStatement()->accept(*this);
  else
    ifStatement.trueStatement().accept(*this);

  // If we have an if-false block, then we need a new join block to jump to.
  if (HasIfFalse) {
    iele::IeleBlock *IfTrueBlock = JoinBlock;
    JoinBlock = iele::IeleBlock::Create(&Context, "if.end", CompilingFunction);

    connectWithUnconditionalJump(ConditionBlock, JoinBlock);

    // Append the code for the if-true block.
    CompilingBlock = IfTrueBlock;
    ifStatement.trueStatement().accept(*this);
  }

  // Compilation continues in the join block.
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
      iele::IeleBlock::Create(&Context, "while.cond", CompilingFunction);

  // Create the loop exit block.
  iele::IeleBlock *LoopExitBlock =
    iele::IeleBlock::Create(&Context, "while.end", CompilingFunction);

  if (!whileStatement.isDoWhile()) {
    // In a while loop, we first Visit the condition.
    iele::IeleValue * ConditionValue =
      compileExpression(whileStatement.condition());
    assert(ConditionValue &&
           "IeleCompiler: Failed to compile while condition.");

    // Invert the condition.
    iele::IeleLocalVariable *InvConditionValue =
      iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
    iele::IeleInstruction::CreateIsZero(InvConditionValue, ConditionValue,
                                        CompilingBlock);
    ConditionValue = InvConditionValue;

    // Branch out of the loop if the condition doesn't hold.
    connectWithConditionalJump(ConditionValue, LoopBodyBlock, LoopExitBlock);
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
    CompilingBlock = LoopCondBlock;
    iele::IeleValue * ConditionValue =
      compileExpression(whileStatement.condition());
    assert(ConditionValue &&
           "IeleCompiler: Failed to compile do-while condition.");

    // Branch to the start of the loop if the condition holds.
    connectWithConditionalJump(ConditionValue, LoopCondBlock, LoopBodyBlock);
  } else {
    // In a while loop, we jump to the start of the loop unconditionally.
    connectWithUnconditionalJump(LoopBodyBlock, LoopBodyBlock);
  }

  // Compilation continues in the loop exit block.
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
    LoopIncBlock =
      iele::IeleBlock::Create(&Context, "for.inc", CompilingFunction);

  // Create the loop exit block.
  iele::IeleBlock *LoopExitBlock =
    iele::IeleBlock::Create(&Context, "for.end", CompilingFunction);

  // Visit condition, if it exists.
  if (forStatement.condition()) {
    iele::IeleValue * ConditionValue =
      compileExpression(*forStatement.condition());
    assert(ConditionValue && "IeleCompiler: Failed to compile for condition.");

    // Invert the condition.
    iele::IeleLocalVariable *InvConditionValue =
      iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
    iele::IeleInstruction::CreateIsZero(InvConditionValue, ConditionValue,
                                        CompilingBlock);
    ConditionValue = InvConditionValue;

    // Branch out of the loop if the condition doesn't hold.
    connectWithConditionalJump(ConditionValue, LoopBodyBlock, LoopExitBlock);
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
    CompilingBlock = LoopIncBlock;
    forStatement.loopExpression()->accept(*this);
  }

  // Jump back to the start of the loop.
  connectWithUnconditionalJump(CompilingBlock, LoopBodyBlock);

  // Compilation continues in the loop exit block.
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
  assert(false && "not implemented yet");
  return false;
}

bool IeleCompiler::visit(const ExpressionStatement &expressionStatement) {
  compileExpression(expressionStatement.expression());
  return false;
}

bool IeleCompiler::visit(const PlaceholderStatement &placeholderStatement) {
  assert(false && "not implemented yet");
  return false;
}

bool IeleCompiler::visit(const InlineAssembly &inlineAssembly) {
  assert(false && "not implemented yet");
  return false;
}

bool IeleCompiler::visit(const Conditional &condition) {
  assert(false && "not implemented yet");
  return false;
}

bool IeleCompiler::visit(const Assignment &assignment) {
  Token::Value op = assignment.assignmentOperator();
  assert(op == Token::Assign && "not implemented yet");
  assert(assignment.leftHandSide().annotation().type->category() !=
	         Type::Category::Tuple && "not implemented yet");

  // Visit LHS.
  iele::IeleValue *LHSValue = compileLValue(assignment.leftHandSide());
  assert(LHSValue && "IeleCompiler: Failed to compile LHS of assignment");

  // Visit RHS and store it as the result of the expression.
  iele::IeleValue *RHSValue = compileExpression(assignment.rightHandSide());
  assert(RHSValue && "IeleCompiler: Failed to compile RHS of assignment");
  CompilingExpressionResult.push_back(RHSValue);

  // If the LHS is a global variable we need to perform an sstore, else a simple
  // assignment.
  if (iele::IeleGlobalVariable *GV =
        llvm::dyn_cast<iele::IeleGlobalVariable>(LHSValue))
    iele::IeleInstruction::CreateSStore(RHSValue, GV, CompilingBlock);
  else
    iele::IeleInstruction::CreateAssign(
      llvm::cast<iele::IeleLocalVariable>(LHSValue), RHSValue, CompilingBlock);

  return false;
}

bool IeleCompiler::visit(const TupleExpression &tuple) {
  assert(false && "not implemented yet");
  return false;
}

bool IeleCompiler::visit(const UnaryOperation &unaryOperation) {
  // Visit subexpression.
  iele::IeleValue *SubExprValue = 
    compileExpression(unaryOperation.subExpression());
  assert(SubExprValue && "IeleCompiler: Failed to compile operand.");

  switch (unaryOperation.getOperator()) {
  case Token::Not: {// !
    iele::IeleLocalVariable *Result =
      iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
    iele::IeleInstruction::CreateIsZero(Result, SubExprValue, CompilingBlock);
    CompilingExpressionResult.push_back(Result);
    break;
  }
  case Token::Add: // +
    // unary add, so basically no-op
    CompilingExpressionResult.push_back(SubExprValue);
    break;
  case Token::Sub: { // -
    iele::IeleLocalVariable *Result =
      iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
    iele::IeleInstruction::CreateBinOp(iele::IeleInstruction::Sub,
                                       Result,
                                       iele::IeleIntConstant::getZero(&Context),
                                       SubExprValue, CompilingBlock);
    CompilingExpressionResult.push_back(Result);
    break;
  }
  case Token::BitNot: // ~
  case Token::Delete: // delete
  case Token::Inc: // ++ (pre- or postfix)
  case Token::Dec: // -- (pre- or postfix)
    assert(false && "not implemented yet");
    break;
  default:
    assert(false && "IeleCompiler: Invalid unary operator");
    break;
  }

  return false;
}

bool IeleCompiler::visit(const BinaryOperation &binaryOperation) {
  // Visit operands.
  iele::IeleValue *LeftOperandValue = 
    compileExpression(binaryOperation.leftExpression());
  assert(LeftOperandValue && "IeleCompiler: Failed to compile left operand.");
  iele::IeleValue *RightOperandValue = 
    compileExpression(binaryOperation.rightExpression());
  assert(RightOperandValue && "IeleCompiler: Failed to compile right operand.");

  // Find corresponding IELE binary opcode.
  iele::IeleInstruction::IeleOps BinOpcode;
  switch (binaryOperation.getOperator()) {
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
    assert(false && "not implemented yet");
    assert(false && "IeleCompiler: Invalid binary operator");
  }

  // Create the instruction.
  iele::IeleLocalVariable *Result =
    iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
  iele::IeleInstruction::CreateBinOp(BinOpcode,
                                     Result,
                                     LeftOperandValue,
                                     RightOperandValue, CompilingBlock);

  CompilingExpressionResult.push_back(Result);
  return false;
}

bool IeleCompiler::visit(const FunctionCall &functionCall) {
  // Not supported yet cases.
  if (functionCall.annotation().kind == FunctionCallKind::TypeConversion) {
    assert(false && "not implemented yet");
    return false;
  }

  if (functionCall.annotation().kind ==
        FunctionCallKind::StructConstructorCall) {
    assert(false && "not implemented yet");
    return false;
  }

  if (!functionCall.names().empty()) {
    assert(false && "not implemented yet");
    return false;
  }

  FunctionTypePointer functionType =
    std::dynamic_pointer_cast<const FunctionType>(
        functionCall.expression().annotation().type);
  const FunctionType &function = *functionType;
  const std::vector<ASTPointer<const Expression>> &arguments =
    functionCall.arguments();
  switch (function.kind()) {
  case FunctionType::Kind::Send:
  case FunctionType::Kind::Transfer: {
    // Get target address.
    iele::IeleValue *TargetAddressValue =
      compileExpression(functionCall.expression());
    assert(TargetAddressValue &&
           "IeleCompiler: Failed to compile transfer target address.");

    // Get transfer value.
    iele::IeleValue *TransferValue =
      compileExpression(*arguments.front().get());
    assert(TransferValue &&
           "IeleCompiler: Failed to compile transfer value.");

    llvm::SmallVector<iele::IeleValue *, 0> EmptyArguments;

    // Create computation for gas
    iele::IeleLocalVariable *GasValue =
      iele::IeleLocalVariable::Create(&Context, "gas", CompilingFunction);
    iele::IeleInstruction::CreateIntrinsicCall(
       iele::IeleInstruction::Gaslimit, GasValue, EmptyArguments,
       CompilingBlock);

    // Create call to deposit.
    iele::IeleValueSymbolTable *ST =
      CompilingContract->getIeleValueSymbolTable();
    assert(ST &&
           "IeleCompiler: failed to access compiling contract's symbol table.");
    iele::IeleValue *DepositValue = ST->lookup("deposit");
    assert(DepositValue && llvm::isa<iele::IeleFunction>(DepositValue) &&
           "IeleCompiler: @deposit function not found.");
    iele::IeleFunction *Deposit = llvm::cast<iele::IeleFunction>(DepositValue);
    iele::IeleLocalVariable *StatusValue =
      iele::IeleLocalVariable::Create(&Context, "status", CompilingFunction);
    iele::IeleInstruction::CreateAccountCall(StatusValue, Deposit,
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
    assert(ConditionValue &&
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
  case FunctionType::Kind::ObjectCreation:
  case FunctionType::Kind::Log0:
  case FunctionType::Kind::Log1:
  case FunctionType::Kind::Log2:
  case FunctionType::Kind::Log3:
  case FunctionType::Kind::Log4:
  case FunctionType::Kind::Event:
  case FunctionType::Kind::Selfdestruct:
  case FunctionType::Kind::SHA3:
  case FunctionType::Kind::BlockHash:
  case FunctionType::Kind::AddMod:
  case FunctionType::Kind::MulMod:
  case FunctionType::Kind::ECRecover:
  case FunctionType::Kind::SHA256:
  case FunctionType::Kind::RIPEMD160:
    assert(false && "not implemented yet");
    break;
  default:
      assert(false && "IeleCompiler: Invalid function type.");
  }

  return false;
}

bool IeleCompiler::visit(const NewExpression &newExpression) {
  assert(false && "not implemented yet");
  return false;
}

bool IeleCompiler::visit(const MemberAccess &memberAccess) {
   const std::string& member = memberAccess.memberName();

  // Not supported yet cases.
  //if (dynamic_cast<const FunctionType *>(
  //        memberAccess.annotation().type.get())) {
  //  assert(false && "not implemented yet");
  //  return false;
  //}

  if (dynamic_cast<const TypeType *>(
          memberAccess.expression().annotation().type.get())) {
    assert(false && "not implemented yet");
    return false;
  }

  // Visit accessed exression.
  iele::IeleValue *ExprValue = compileExpression(memberAccess.expression());

  switch (memberAccess.expression().annotation().type->category()) {
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
    } else if (member == "coinbase")
      assert(false && "IeleCompiler: member not supported in IELE");
    else if (member == "data")
      assert(false && "IeleCompiler: member not supported in IELE");
    else if (member == "sig")
      assert(false && "IeleCompiler: member not supported in IELE");
    else
      assert(false && "IeleCompiler: Unknown magic member.");
    break;
  case Type::Category::Integer:
    assert((member == "transfer" || member == "send") &&
           "not implemented yet");
    // Simply forward the result (which should be an address).
    assert(ExprValue && "IeleCompiler: Failed to compile address");
    CompilingExpressionResult.push_back(ExprValue);
    break;
  case Type::Category::Contract:
  case Type::Category::Function:
  case Type::Category::Struct:
  case Type::Category::Enum:
  case Type::Category::Array:
  case Type::Category::FixedBytes:
    assert(false && "not implemented yet");
  default:
    assert(false && "IeleCompiler: Member access to unknown type.");
  }

  return false;
}

bool IeleCompiler::visit(const IndexAccess &indexAccess) {
  assert(false && "not implemented yet");
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
      assert(false && "not implemented yet");
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
  assert(ST &&
         "IeleCompiler: failed to access compiling function's symbol table.");
  if (iele::IeleValue *Identifier = ST->lookup(name)) {
    CompilingExpressionResult.push_back(Identifier);
    return;
  }

  // If not found, lookup identifier in the contract's symbol table.
  ST = CompilingContract->getIeleValueSymbolTable();
  assert(ST &&
         "IeleCompiler: failed to access compiling contract's symbol table.");
  if (iele::IeleValue *Identifier = ST->lookup(name)) {
    // If we aren't compiling an lvalue, we have to load the global variable.
    if (!CompilingLValue) {
      if (iele::IeleGlobalVariable *GV =
            llvm::dyn_cast<iele::IeleGlobalVariable>(Identifier)) {
        iele::IeleLocalVariable *LoadedValue =
          iele::IeleLocalVariable::Create(&Context, name + ".val",
                                          CompilingFunction);
        iele::IeleInstruction::CreateSLoad(LoadedValue, GV, CompilingBlock);
        CompilingExpressionResult.push_back(LoadedValue);
        return;
      }
    }

    CompilingExpressionResult.push_back(Identifier);
    return;
  }

  // If not found, make a new IeleLocalVariable for the identifier.
  iele::IeleLocalVariable *Identifier =
    iele::IeleLocalVariable::Create(&Context, name, CompilingFunction);
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
    assert(false && "not implemented yet");
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
  assert(CompilingExpressionResult.size() > 0);
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
  assert(CompilingExpressionResult.size() > 0);
  Result = CompilingExpressionResult[0];

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
    assert(false && "not implemented yet");
  }

  if (Condition)
    connectWithConditionalJump(Condition, CompilingBlock, AssertFailBlock);
  else
    connectWithUnconditionalJump(CompilingBlock, AssertFailBlock);
}
