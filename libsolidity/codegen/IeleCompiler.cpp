#include "libsolidity/ast/TypeProvider.h"

#include "libsolidity/codegen/IeleCompiler.h"
#include "libsolidity/codegen/IeleLValue.h"

#include <liblangutil/Exceptions.h>

#include <libsolutil/Keccak256.h>

#include "libiele/IeleContract.h"
#include "libiele/IeleGlobalVariable.h"
#include "libiele/IeleIntConstant.h"

#include <iostream>
#include "llvm/Support/raw_ostream.h"

using namespace solidity;
using namespace solidity::frontend;

auto UInt = TypeProvider::uint();
auto UInt16 = TypeProvider::uint(16);
auto SInt = TypeProvider::sint();
auto Address = TypeProvider::address();

IeleRValue *IeleCompiler::Value::rval(iele::IeleBlock *Block) {
  return isLValue ? LValue->read(Block) : RValue;
}

std::string IeleCompiler::getIeleNameForFunction(
    const FunctionDefinition &function) {
  std::string IeleFunctionName;
  if (function.isConstructor())
    IeleFunctionName = "init";
  else if (function.isFallback())
    IeleFunctionName = "fallback";
  else if (function.isReceive())
    IeleFunctionName = "receive";
  else if (function.isPublic())
    IeleFunctionName = function.externalSignature();
  else
    IeleFunctionName = function.name() + "." + function.type()->identifier();

  if (isMostDerived(&function)) {
    return IeleFunctionName;
  } else {
    return getIeleNameForContract(contractFor(&function)) + "." + IeleFunctionName;
  }
}

std::string IeleCompiler::getIeleNameForLocalVariable(
    const VariableDeclaration *localVariable) {
  solAssert(localVariable->isLocalVariable() &&
            !localVariable->isCallableOrCatchParameter(),
            "IeleCompiler: requested unique local variable name for a "
            "non-local variable.");
  return localVariable->name() + "." +
         std::to_string(localVariable->location().start) + "." +
         std::to_string(localVariable->location().end);
}

std::string IeleCompiler::getIeleNameForStateVariable(
    const VariableDeclaration *stateVariable) {
  if (isMostDerived(stateVariable)) {
    return stateVariable->name();
  } else {
    return getIeleNameForContract(contractFor(stateVariable)) + "." + stateVariable->name();
  }
}

std::string IeleCompiler::getIeleNameForAccessor(
    const VariableDeclaration *stateVariable) {
  FunctionType accessorType(*stateVariable);
  if (isMostDerived(stateVariable)) {
    return accessorType.externalSignature();
  } else {
    return getIeleNameForContract(contractFor(stateVariable)) + "." + accessorType.externalSignature();
  }
}

std::string IeleCompiler::getIeleNameForContract(
    const ContractDefinition *contract) {
  return contract->fullyQualifiedName();
}

// lookup a ModifierDefinition by name (borrowed from CompilerContext.cpp)
const ModifierDefinition *IeleCompiler::functionModifier(
    const std::string &_name) const {
  solAssert(!CompilingContractInheritanceHierarchy.empty(), "CurrentContract not set.");
  for (const ContractDefinition *CurrentContract : CompilingContractInheritanceHierarchy) {
    for (ModifierDefinition const* modifier: CurrentContract->functionModifiers())
      if (modifier->name() == _name)
        return modifier;
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
      if (d->name() == decl->name() && !decl->isConstructor() && FunctionType(*decl).hasEqualParameterTypes(FunctionType(*d))) {
        return d == decl;
      }
    }
  }
  return false;
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
  return false;
}

const ContractDefinition *IeleCompiler::contractFor(const Declaration *d) const {
  return dynamic_cast<const ContractDefinition *>(d->scope());
}

const FunctionDefinition *IeleCompiler::resolveVirtualFunction(const FunctionDefinition &function, std::vector<const ContractDefinition *>::iterator it) {
  for (; it != CompilingContractInheritanceHierarchy.end(); it++) {
    const ContractDefinition *contract = *it;
    for (const FunctionDefinition *decl : contract->definedFunctions()) {
      if (function.name() == decl->name() && !decl->isConstructor() && FunctionType(*decl).hasEqualParameterTypes(FunctionType(function))) {
        return decl;
      }
    }
  }
  solAssert(false, "Function definition not found.");
  return nullptr; // not reached
}

const FunctionDefinition *IeleCompiler::resolveVirtualFunction(const FunctionDefinition &function) {
  solAssert(!CompilingContractInheritanceHierarchy.empty(), "IeleCompiler: current contract not set.");
  return resolveVirtualFunction(function, CompilingContractInheritanceHierarchy.begin());
}

const FunctionDefinition *IeleCompiler::superFunction(const FunctionDefinition &function, const ContractDefinition &contract) {
  solAssert(!CompilingContractInheritanceHierarchy.empty(), "IeleCompiler: current contract not set.");

  auto it = find(CompilingContractInheritanceHierarchy.begin(), CompilingContractInheritanceHierarchy.end(), &contract);
  solAssert(it != CompilingContractInheritanceHierarchy.end(), "Base not found in inheritance hierarchy.");
  it++;

  return resolveVirtualFunction(function, it);
}

const ContractDefinition *IeleCompiler::superContract(const ContractDefinition &contract) {
  solAssert(!CompilingContractInheritanceHierarchy.empty(), "IeleCompiler: current contract not set.");

  auto it = find(CompilingContractInheritanceHierarchy.begin(), CompilingContractInheritanceHierarchy.end(), &contract);
  solAssert(it != CompilingContractInheritanceHierarchy.end(), "Contract not found in inheritance hierarchy.");
  it++;
  solAssert(it != CompilingContractInheritanceHierarchy.end(), "Base not found in inheritance hierarchy.");

  return *it;
}

static bool hasTwoFunctions(const FunctionType &function, bool isConstructor, bool isLibrary) {
  if (isConstructor || isLibrary) {
    return false;
  }
  if (!function.hasDeclaration() || function.declaration().visibility() != Visibility::Public) {
    return false;
  }
  for (TypePointer type : function.parameterTypes()) {
    if (!type->isValueType()) {
      return true;
    }
  }
  for (TypePointer type : function.returnParameterTypes()) {
    if (!type->isValueType()) {
      return true;
    }
  }
  return false;
}

iele::IeleValue *IeleCompiler::convertFunctionToInternal(iele::IeleValue *Callee) {
  if (iele::IeleFunction *function = llvm::dyn_cast<iele::IeleFunction>(Callee)) {
    iele::IeleValueSymbolTable *ST = CompilingContract->getIeleValueSymbolTable();
    solAssert(ST,
              "IeleCompiler: failed to access compiling contract's symbol table.");
    std::string name = function->getName().str();
    iele::IeleValue *internalName = ST->lookup(name + ".internal");
    if (internalName) {
      return llvm::dyn_cast<iele::IeleGlobalValue>(internalName);
    }
  }
  return Callee;
}

static void transitiveClosure(const ContractDefinition *contract, std::set<const ContractDefinition *> &dependencies) {
  for (const ContractDefinition *dependency : contract->annotation().contractDependencies) {
    dependencies.insert(dependency);
    transitiveClosure(dependency, dependencies);
  }
}
static void transitiveClosure(std::set<const ContractDefinition *> &dependencies) {
  for (const ContractDefinition *contract : dependencies) {
    transitiveClosure(contract, dependencies);
  }
}


namespace {

class RecursiveStructLocator : public ASTConstVisitor {
public:
  RecursiveStructLocator() : RecursiveStructFound(false) { }

  virtual bool visit(const StructDefinition &structDefinition) override {
    TypePointer type = structDefinition.type();
    solAssert(type->category() == Type::Category::TypeType, "");
    const TypeType &typeType = dynamic_cast<const TypeType &>(*type);
    type = typeType.actualType();
    solAssert(type->category() == Type::Category::Struct, "");
    const StructType &structType = dynamic_cast<const StructType &>(*type);
    if (structType.recursive())
      RecursiveStructFound = true;
    return true;
  }

  bool recursiveStructFound() const { return RecursiveStructFound; }

private:
  bool RecursiveStructFound;
};

static bool checkForRecursiveStructs(const ContractDefinition &contract) {
  RecursiveStructLocator Locator;
  contract.accept(Locator);
  return Locator.recursiveStructFound();
}

} // end anonymous namespace

void IeleCompiler::compileContract(
    const ContractDefinition &contract,
    const std::map<const ContractDefinition *, std::shared_ptr<IeleCompiler const>> &otherCompilers,
    const bytes &metadata) {


  OtherCompilers = otherCompilers;

  // Create IeleContract.
  CompilingContract = iele::IeleContract::Create(&Context, getIeleNameForContract(&contract));

  // Check for recursive structs. Their presence will affect how we allocate
  // storage for the contract.
  bool recursiveStructsFound = checkForRecursiveStructs(contract);
  MappingType::setInfiniteKeyspaceMappingsSuppression(recursiveStructsFound);

  // Add IELE global variables and functions to contract's symbol table by
  // iterating over state variables and functions of this contract and its base
  // contracts.
  std::vector<ContractDefinition const*> bases =
    contract.annotation().linearizedBaseContracts;
  // Store the current contract.
  CompilingContractInheritanceHierarchy = bases;
  bool most_derived = true;
  const FunctionDefinition *MostDerivedFallback = nullptr;
  const FunctionDefinition *MostDerivedReceive = nullptr;
  
  // Compute and store info for handling multiple inheritance
  computeCtorsAuxParams();

  for (const ContractDefinition *base : bases) {
    // Add global variables corresponding to the current contract in the
    // inheritance hierarchy.
    for (const VariableDeclaration *stateVariable : base->stateVariables()) {
      std::string VariableName = getIeleNameForStateVariable(stateVariable);
      if (!stateVariable->isConstant()) {
        iele::IeleGlobalVariable *GV =
          iele::IeleGlobalVariable::Create(&Context, VariableName,
                                           CompilingContract);
        GV->setStorageAddress(iele::IeleIntConstant::Create(
                                  &Context, NextStorageAddress));
      }

      if (stateVariable->isPublic()) {
        std::string AccessorName = getIeleNameForAccessor(stateVariable);
        iele::IeleFunction::Create(&Context, true, AccessorName,
                                   CompilingContract);
      }

      NextStorageAddress += stateVariable->annotation().type->storageSize();
    }

    // Add a constructor function corresponding to the current contract in the
    // inheritance hierarchy.
    if (base->constructor()) {
      if (most_derived) {
        // This is the actual constructor, add it to the symbol table.
        iele::IeleFunction::CreateInit(&Context, CompilingContract);
      } else {
        // Generate a proxy function for the base contract's constructor and add
        // it to the symbol table.
        iele::IeleFunction::Create(&Context, false, getIeleNameForContract(base) + ".init",
                                   CompilingContract);
      }
    }

    // Add the rest of the functions found in the current contract of the
    // inheritance hierarchy.
    for (const FunctionDefinition *function : base->definedFunctions()) {
      if (function->isConstructor() || !function->isImplemented())
        continue;
      if (!MostDerivedFallback && function->isFallback())
        MostDerivedFallback = function;
      if (!MostDerivedReceive && function->isReceive())
        MostDerivedReceive = function;
      std::string FunctionName = getIeleNameForFunction(*function);
      iele::IeleFunction::Create(&Context, function->isPublic(),
                                 FunctionName, CompilingContract);
      // here we don't call isPublic because we want to exclude external functions
      if (hasTwoFunctions(FunctionType(*function), false, false)) {
        iele::IeleFunction::Create(&Context, false, FunctionName + ".internal", CompilingContract);
      }
    }
    most_derived = false;
  }

  // Add functions from imported libraries.
  std::set<ContractDefinition const*> dependencies = contract.annotation().contractDependencies;
  transitiveClosure(dependencies);
  for (auto dep : dependencies) {
    if (dep->isLibrary()) {
      for (const FunctionDefinition *function : dep->definedFunctions()) {
        if (function->isConstructor() || function->isFallback() ||
            !function->isImplemented())
          continue;
        std::string FunctionName = getIeleNameForFunction(*function);
        iele::IeleFunction::Create(&Context, function->isPublic(),
                                   FunctionName, CompilingContract);
      }
    }
  }

  // Finally add the default deposit function to the contract's symbol table.
  if (MostDerivedFallback || MostDerivedReceive) {
    CompilingFunction =
      iele::IeleFunction::CreateDeposit(&Context, true, CompilingContract);
    CompilingBlock =
      iele::IeleBlock::Create(&Context, "entry", CompilingFunction);

    // Choose between receive and fallback as the implmentation of the
    // deposit function
    const FunctionDefinition *DepositFunction =
      MostDerivedReceive ? MostDerivedReceive : MostDerivedFallback;
    std::string name = getIeleNameForFunction(*DepositFunction);

    // Lookup function in the contract's symbol table.
    iele::IeleValueSymbolTable *ST = CompilingContract->getIeleValueSymbolTable();
    solAssert(ST,
              "IeleCompiler: failed to access compiling contract's symbol table.");
    iele::IeleFunction *Callee = llvm::dyn_cast<iele::IeleFunction>(ST->lookup(name));

    // Append check for payable
    if (!DepositFunction->isPayable()) {
      appendPayableCheck();
    }

    // Create the function call to the fallback/receive internal function.
    llvm::SmallVector<iele::IeleValue *, 4> paramValues;
    llvm::SmallVector<iele::IeleLocalVariable *, 4> ReturnParameterRegisters;
    solAssert(DepositFunction->parameters().size() == 0,
              "IeleCompiler: fallback/receive function with non-zero number"
              "of arguments is not currently supported.");
    solAssert(DepositFunction->returnParameters().size() == 0,
              "IeleCompiler: fallback/receive function with non-zero number"
              "of return values is not currently supported.");
    iele::IeleInstruction::CreateInternalCall(
      ReturnParameterRegisters, Callee, paramValues, CompilingBlock);

    // Create ret void
    iele::IeleInstruction::CreateRetVoid(CompilingBlock);
    appendRevertBlocks();

    CompilingBlock = nullptr;
    CompilingFunction = nullptr;
  }

  // Visit base constructors. If any don't exist create an
  // @<base-contract-name>init function that contains only state variable
  // initialization.
  for (auto it = bases.rbegin(); it != bases.rend(); it++) {
    const ContractDefinition *base = *it;
    CompilingContractASTNode = base;
    if (base == &contract) {
      continue;
    }
    VarNameMap.clear();
    if (const FunctionDefinition *constructor = base->constructor()) {
      constructor->accept(*this);
    } else {
      CompilingFunction =
        iele::IeleFunction::Create(&Context, false, getIeleNameForContract(base) + ".init", CompilingContract);
      CompilingFunctionStatus = iele::IeleLocalVariable::Create(&Context, "status", CompilingFunction);
      CompilingBlock =
        iele::IeleBlock::Create(&Context, "entry", CompilingFunction);

      // Add constructor's aux params
      auto auxParams = ctorAuxParams[CompilingContractASTNode];
      if (!auxParams.empty()) {
        // Make iterator
        std::map<const ContractDefinition *, 
                std::pair<std::vector<std::string>, const ContractDefinition *>>::iterator it;

        for(it = auxParams.begin(); it != auxParams.end(); ++it) {
          auto forwardingAuxParams = it -> second;
          auto paramNames = forwardingAuxParams.first;

          for (std::string paramName : paramNames) {
            iele::IeleArgument *arg = iele::IeleArgument::Create(&Context, paramName, CompilingFunction);
            VarNameMap[0][paramName] = RegisterLValue::Create({arg});
          }
        }
      }
      
      appendDefaultConstructor(CompilingContractASTNode);

      iele::IeleInstruction::CreateRetVoid(CompilingBlock);
      appendRevertBlocks();
      CompilingBlock = nullptr;
      CompilingFunction = nullptr;
    }
  }

  CompilingContractASTNode = &contract;

  VarNameMap.clear();
  // Similarly visit the contract's constructor.
  if (const FunctionDefinition *constructor = contract.constructor())
    constructor->accept(*this);
  else {
    CompilingFunction =
      iele::IeleFunction::CreateInit(&Context, CompilingContract);
    CompilingFunctionStatus = iele::IeleLocalVariable::Create(&Context, "status", CompilingFunction);
    CompilingBlock =
      iele::IeleBlock::Create(&Context, "entry", CompilingFunction);
    
    // Add constructor's aux params
    auto auxParams = ctorAuxParams[CompilingContractASTNode];
    if (!auxParams.empty()) {
      // Make iterator
      std::map<const ContractDefinition *, 
              std::pair<std::vector<std::string>, const ContractDefinition *>>::iterator it;

      for(it = auxParams.begin(); it != auxParams.end(); ++it) {
        auto forwardingAuxParams = it -> second;
        auto paramNames = forwardingAuxParams.first;

        for (std::string paramName : paramNames) {
          iele::IeleArgument *arg = iele::IeleArgument::Create(&Context, paramName, CompilingFunction);
          VarNameMap[0][paramName] = RegisterLValue::Create({arg});
        }
      }
    }
    
    appendDefaultConstructor(CompilingContractASTNode);
    iele::IeleInstruction::CreateRetVoid(CompilingBlock);
    appendRevertBlocks();
    CompilingBlock = nullptr;
    CompilingFunction = nullptr;
  }

  // Visit functions from imported libraries.
  for (auto dep : dependencies) {
    if (dep->isLibrary()) {
      for (const FunctionDefinition *function : dep->definedFunctions()) {
        VarNameMap.clear();
        if (function->isConstructor() || function->isFallback() ||
            function->isReceive() || !function->isImplemented())
          continue;
        function->accept(*this);
      }
    }
  }

  // Visit functions.
  for (const ContractDefinition *base : bases) {
    CompilingContractASTNode = base;
    for (const FunctionDefinition *function : base->definedFunctions()) {
      VarNameMap.clear();
      if (function->isConstructor() || !function->isImplemented())
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

  // If we have to include the runtime storage, append code that initializes
  // @ielert.storage.next.free global variable at the start of the constructor.
  if (CompilingContract->getIncludeStorageRuntime()) {
    CompilingContract->setStorageRuntimeNextFreePtrAddress(NextStorageAddress);
    iele::IeleValueSymbolTable *ST =
      CompilingContract->getIeleValueSymbolTable();
    solAssert(ST,
              "IeleCompiler: failed to access compiling contract's symbol "
              "table.");
    iele::IeleFunction *InitFunction =
       llvm::dyn_cast<iele::IeleFunction>(ST->lookup("init"));
    solAssert(InitFunction,
              "IeleCompiler: failed to find init function in compiling "
              "contract's symbol table");
    iele::IeleGlobalVariable *NextFree =
      iele::IeleGlobalVariable::Create(&Context, "ielert.storage.next.free");
    iele::IeleInstruction::CreateSStore(
        iele::IeleIntConstant::Create(&Context, NextStorageAddress + 1),
        NextFree, &InitFunction->front().front());
  }

  // Store compilation result.
  CompiledContract = CompilingContract;

  // Append the metadata to the compiled contract.
  CompiledContract->appendAuxiliaryDataToEnd(metadata);
}

int IeleCompiler::getNextUniqueIntToken() {
  return NextUniqueIntToken++;
}

std::string IeleCompiler::getNextVarSuffix() {
  return ("_" + std::to_string(getNextUniqueIntToken()));
}

void IeleCompiler::appendAccessorFunction(const VariableDeclaration *stateVariable) {
  std::string name = getIeleNameForStateVariable(stateVariable);
  std::string accessorName = getIeleNameForAccessor(stateVariable);

   // Lookup function in the contract's symbol table.
  iele::IeleValueSymbolTable *ST = CompilingContract->getIeleValueSymbolTable();
  solAssert(ST,
            "IeleCompiler: failed to access compiling contract's symbol table.");
  CompilingFunction = llvm::dyn_cast<iele::IeleFunction>(ST->lookup(accessorName));
  solAssert(CompilingFunction,
            "IeleCompiler: failed to find function in compiling contract's"
            " symbol table");

  // Create the entry block.
  CompilingBlock =
    iele::IeleBlock::Create(&Context, "entry", CompilingFunction);

  appendPayableCheck();

  llvm::SmallVector<iele::IeleValue *, 4> Returns;
  if (stateVariable->isConstant()) {
    IeleRValue *LoadedValue = compileExpression(*stateVariable->value());
    TypePointer rhsType = stateVariable->value()->annotation().type;
    LoadedValue = appendTypeConversion(LoadedValue, rhsType, stateVariable->annotation().type);
    if (auto arrayType = dynamic_cast<const ArrayType *>(stateVariable->annotation().type)) {
      if (arrayType->isByteArray()) {
        LoadedValue = encoding(LoadedValue, stateVariable->annotation().type);
      } else {
        solAssert(false, "constant non-byte array");
      }
    }
    Returns.insert(Returns.end(), LoadedValue->getValues().begin(), LoadedValue->getValues().end());
  } else {
    iele::IeleGlobalVariable *GV = llvm::dyn_cast<iele::IeleGlobalVariable>(ST->lookup(name));

    FunctionType accessorType(*stateVariable);
    TypePointers paramTypes = accessorType.parameterTypes();

    std::vector<iele::IeleArgument *> parameters;
  
    // Visit formal arguments.
    for (unsigned i = 0; i < paramTypes.size(); i++) {
      std::string genName = getNextVarSuffix();
      parameters.push_back(iele::IeleArgument::Create(&Context, genName, CompilingFunction));
    }

    TypePointer returnType = stateVariable->annotation().type;

    IeleRValue *Value = makeLValue(GV, returnType, DataLocation::Storage)->read(CompilingBlock);

    for (unsigned i = 0; i < paramTypes.size(); i++) {
      if (auto mappingType = dynamic_cast<const MappingType *>(returnType)) {
        Value = appendMappingAccess(*mappingType, parameters[i], Value->getValue())->read(CompilingBlock);
        returnType = mappingType->valueType();
      } else if (auto arrayType = dynamic_cast<const ArrayType *>(returnType)) {
        Value = appendArrayAccess(*arrayType, parameters[i], Value->getValue(), DataLocation::Storage)->read(CompilingBlock);
        returnType = arrayType->baseType();
      } else {
        solAssert(false, "Index access is allowed only for \"mapping\" and \"array\" types.");
      }
    }
    auto returnTypes = accessorType.returnParameterTypes();
    solAssert(returnTypes.size() >= 1, "accessor must return a type");
    if (auto structType = dynamic_cast<const StructType *>(returnType)) {
      auto const& names = accessorType.returnParameterNames();
      for (unsigned i = 0; i < names.size(); i++) {
        TypePointer memberType = structType->memberType(names[i]);
        IeleRValue *LoadedValue = appendStructAccess(*structType, Value->getValue(), names[i], DataLocation::Storage)->read(CompilingBlock);
        LoadedValue = encoding(LoadedValue, memberType);
        Returns.insert(Returns.end(), LoadedValue->getValues().begin(), LoadedValue->getValues().end());
      }
    } else {
      Value = encoding(Value, returnType);
      Returns.insert(Returns.end(), Value->getValues().begin(), Value->getValues().end());
    }
  }
  
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
  CompilingFunctionASTNode = &function;

  if (hasTwoFunctions(FunctionType(function), function.isConstructor(), contractFor(&function)->isLibrary())) {
    CompilingFunction = llvm::dyn_cast<iele::IeleFunction>(ST->lookup(name));
    solAssert(CompilingFunction,
              "IeleCompiler: failed to find function in compiling contract's"
              " symbol table");
    name = name + ".internal";
    llvm::SmallVector<IeleLValue *, 4> parameters;
    llvm::SmallVector<iele::IeleValue *, 4> paramValues;
    // Visit formal arguments.
    for (const ASTPointer<const VariableDeclaration> arg : function.parameters()) {
      std::vector<iele::IeleLocalVariable *> param;
      for (unsigned i = 0; i < arg->type()->sizeInRegisters(); i++) {
        std::string genName = arg->name() + getNextVarSuffix();
        auto reg = iele::IeleArgument::Create(&Context, genName, CompilingFunction);
        param.push_back(reg);
        paramValues.push_back(reg);
      }
      parameters.push_back(RegisterLValue::Create(param));
    }
    llvm::SmallVector<IeleLValue *, 4> ReturnParameters;
    llvm::SmallVector<iele::IeleValue *, 4> ReturnParameterValues;
    llvm::SmallVector<iele::IeleLocalVariable *, 4> ReturnParameterRegisters;
 
    // Visit formal return parameters.
    for (const ASTPointer<const VariableDeclaration> ret : function.returnParameters()) {
      std::vector<iele::IeleLocalVariable *> param;
      for (unsigned i = 0; i < ret->type()->sizeInRegisters(); i++) {
        std::string genName = ret->name() + getNextVarSuffix();
        auto reg = iele::IeleLocalVariable::Create(&Context, genName, CompilingFunction);
        param.push_back(reg);
        ReturnParameterRegisters.push_back(reg);
      }
      ReturnParameters.push_back(RegisterLValue::Create(param));
    }
  
    // Create the entry block.
    CompilingBlock =
      iele::IeleBlock::Create(&Context, "entry", CompilingFunction);
 
    if (!function.isPayable() && isMostDerived(&function)) {
      appendPayableCheck();
    }

    for (unsigned i = 0; i < function.parameters().size(); i++) {
      const auto &arg = *function.parameters()[i];
      IeleLValue *ieleArg = parameters[i];
      const auto &argType = arg.type();
      if (argType->isValueType()) {
        appendRangeCheck(ieleArg->read(CompilingBlock), *argType);
      } else {
        ieleArg->write(decoding(ieleArg->read(CompilingBlock), argType), CompilingBlock);
      }
    }
    iele::IeleFunction *Callee = llvm::dyn_cast<iele::IeleFunction>(ST->lookup(name));
    iele::IeleInstruction::CreateInternalCall(
      ReturnParameterRegisters, Callee, paramValues, CompilingBlock);

    for (unsigned i = 0; i < function.returnParameters().size(); i++) {
      IeleRValue *ret = encoding(ReturnParameters[i]->read(CompilingBlock), function.returnParameters()[i]->type());
      ReturnParameterValues.insert(ReturnParameterValues.end(), ret->getValues().begin(), ret->getValues().end());
    }

    iele::IeleInstruction::CreateRet(ReturnParameterValues, CompilingBlock);
    appendRevertBlocks();
  }

  CompilingFunction = llvm::dyn_cast<iele::IeleFunction>(ST->lookup(name));
  solAssert(CompilingFunction,
            "IeleCompiler: failed to find function in compiling contract's"
            " symbol table");

  unsigned NumOfModifiers = CompilingFunctionASTNode->modifiers().size();

  // We store the formal argument names, which we'll use when generating in-range
  // checks in case of a public function.
  std::vector<IeleLValue *> parameters;
  // returnParameters.clear(); // otherwise, stuff keep getting added, regardless
  //                           // of which function we are in (i.e. it breaks)

  // Visit formal arguments.
  for (const ASTPointer<const VariableDeclaration> arg : function.parameters()) {
    std::vector<iele::IeleLocalVariable *> param;
    for (unsigned i = 0; i < arg->type()->sizeInRegisters(); i++) {
      std::string genName = arg->name() + getNextVarSuffix();
      param.push_back(iele::IeleArgument::Create(&Context, genName, CompilingFunction));
    }
    IeleLValue *lvalue = RegisterLValue::Create(param);
    parameters.push_back(lvalue);
    // No need to keep track of the mapping for omitted args, since they will
    // never be referenced.
    if (!(arg->name() == ""))
       VarNameMap[NumOfModifiers][arg->name()] = lvalue;
  }

  // Add aux constructor params 
  if (function.isConstructor()) {
    auto auxParams = ctorAuxParams[CompilingContractASTNode];
    if (!auxParams.empty()) {
      
      // Make iterator
      std::map<const ContractDefinition *, 
               std::pair<std::vector<std::string>, const ContractDefinition *>>::iterator it;

      for(it = auxParams.begin(); it != auxParams.end(); ++it) {
        auto forwardingAuxParams = it -> second;
        auto paramNames = forwardingAuxParams.first;

        for (std::string paramName : paramNames) {
          if (!paramName.empty()) {
            iele::IeleArgument *arg = iele::IeleArgument::Create(&Context, paramName, CompilingFunction);
            VarNameMap[NumOfModifiers][paramName] = RegisterLValue::Create({arg});
          }
        }
      }
    }
  }

  // We store the return params names, which we'll use when generating a default
  // `ret`.
  llvm::SmallVector<TypePointer, 4> ReturnParameterTypes;

  // Visit formal return parameters.
  for (const ASTPointer<const VariableDeclaration> ret : function.returnParameters()) {
    std::vector<iele::IeleLocalVariable *> param;
    for (unsigned i = 0; i < ret->type()->sizeInRegisters(); i++) {
      std::string genName = ret->name() + getNextVarSuffix();
      param.push_back(iele::IeleLocalVariable::Create(&Context, genName, CompilingFunction));
    }
    ReturnParameterTypes.push_back(ret->type());
    IeleLValue *lvalue = RegisterLValue::Create(param);
    CompilingFunctionReturnParameters.push_back(lvalue);
    // No need to keep track of the mapping for omitted return params, since
    // they will never be referenced.
    if (!(ret->name() == ""))
      VarNameMap[NumOfModifiers][ret->name()] = lvalue; 
  }

  // Visit local variables.
  for (const VariableDeclaration *local: function.localVariables()) {
    std::string localName = getIeleNameForLocalVariable(local);
    std::vector<iele::IeleLocalVariable *> param;
    for (unsigned i = 0; i < local->type()->sizeInRegisters(); i++) {
      std::string genName = localName + getNextVarSuffix();
      param.push_back(iele::IeleLocalVariable::Create(&Context, genName, CompilingFunction));
    }
    IeleLValue *lvalue = RegisterLValue::Create(param);
    VarNameMap[NumOfModifiers][localName] = lvalue;
  }

  CompilingFunctionStatus =
    iele::IeleLocalVariable::Create(&Context, "status", CompilingFunction);

  // Create the entry block.
  CompilingBlock =
    iele::IeleBlock::Create(&Context, "entry", CompilingFunction);

  if (function.stateMutability() > StateMutability::View
      && CompilingContractASTNode->isLibrary()) {
    appendRevert();
  }

  if ((function.isConstructor() || function.isPublic()) &&
      !hasTwoFunctions(FunctionType(function), function.isConstructor(), false) &&
      !contractFor(&function)->isLibrary()) {
    if (!function.isPayable() && isMostDerived(&function)) {
      appendPayableCheck();
    }
    for (unsigned i = 0; i < function.parameters().size(); i++) {
      const auto &arg = *function.parameters()[i];
      IeleLValue *ieleArg = parameters[i];
      const auto &argType = arg.type();
      if (argType->isValueType()) {
        appendRangeCheck(ieleArg->read(CompilingBlock), *argType);
      } else if (!function.isConstructor() || isMostDerived(&function)) {
        ieleArg->write(decoding(ieleArg->read(CompilingBlock), argType), CompilingBlock);
      }
    }
  }
  // If the function is a constructor, visit state variables and add
  // initialization code.
  if (function.isConstructor())
    appendDefaultConstructor(CompilingContractASTNode);

  // Initialize local variables and return params.
  for (const VariableDeclaration *local: function.localVariables()) {
    std::string localName = getIeleNameForLocalVariable(local);
    IeleLValue *Local =
      VarNameMap[NumOfModifiers][localName];
    solAssert(Local, "IeleCompiler: missing local variable");
    appendLocalVariableInitialization(
      Local, local);
  }
  for (unsigned i = 0; i < function.returnParameters().size(); i++) {
    const ASTPointer<const VariableDeclaration> &ret = function.returnParameters()[i];
    IeleLValue *lvalue = CompilingFunctionReturnParameters[i];
    solAssert(lvalue, "IeleCompiler: missing return parameter");
    appendLocalVariableInitialization(
      lvalue, &*ret);
  }

  // Make return block 
  iele::IeleBlock *retBlock = iele::IeleBlock::Create(
    &Context, "return");
  
  // Add it to stack of return locations
  ReturnBlocks.push(retBlock);

  // Visit function body (inc modifiers). 
  CompilingFunctionASTNode = &function;
  ModifierDepth = -1;
  appendModifierOrFunctionCode();

  // Append return block
  appendReturn(function, ReturnParameterTypes);

  // Append the exception blocks if needed.
  appendRevertBlocks();

  CompilingBlock = nullptr;
  CompilingFunction = nullptr;
  CompilingFunctionReturnParameters.clear(); // otherwise, stuff keep getting added, regardless
                            // of which function we are in (i.e. it breaks)
  return false;
}

void IeleCompiler::appendReturn(const FunctionDefinition &function, 
    llvm::SmallVector<TypePointer, 4> ReturnParameterTypes) {

  solAssert(!ReturnBlocks.empty(), "IeleCompiler: appendReturn error");

  auto retBlock = ReturnBlocks.top();

  // Append block
  retBlock -> insertInto(CompilingFunction);
  
  // Set it as currently compiling block
  CompilingBlock = retBlock;

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
      // for (const std::string paramName : ReturnParameterNames) {
      //   iele::IeleValue *param = ST->lookup(paramName);
      //   solAssert(param, "IeleCompiler: couldn't find return parameter name in symbol table:");
      //   Returns.push_back(param);
      // }

      for (unsigned i = 0; i < ReturnParameterTypes.size(); i++) {
          IeleLValue *param = CompilingFunctionReturnParameters[i];
          TypePointer paramType = ReturnParameterTypes[i];
          solAssert(param, "IeleCompiler: couldn't find parameter name in symbol table.");
          if (function.isPublic() && !hasTwoFunctions(FunctionType(function), function.isConstructor(), false) && !contractFor(&function)->isLibrary()) {
            IeleRValue *rvalue = encoding(param->read(CompilingBlock), paramType);
            Returns.insert(Returns.end(), rvalue->getValues().begin(), rvalue->getValues().end());
          } else {
            IeleRValue *rvalue = param->read(CompilingBlock);
            Returns.insert(Returns.end(), rvalue->getValues().begin(), rvalue->getValues().end());
          }
        }

      iele::IeleInstruction::CreateRet(Returns, CompilingBlock);
  }
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

bool IeleCompiler::visit(const Block &block) {
  if (block.unchecked()) {
    solAssert(getArithmetic() == Arithmetic::Checked, "");
    setArithmetic(Arithmetic::Wrapping);
  }
  return true;
}

void IeleCompiler::endVisit(const Block &block) {
  if (block.unchecked()) {
    solAssert(getArithmetic() == Arithmetic::Wrapping, "");
    setArithmetic(Arithmetic::Checked);
  }
}

bool IeleCompiler::visit(const IfStatement &ifStatement) {
  // Check if we have an if-false block. Our compilation strategy depends on
  // that.
  bool HasIfFalse = ifStatement.falseStatement() != nullptr;

  // Visit condition.
  iele::IeleValue *ConditionValue =
    compileExpression(ifStatement.condition())->getValue();
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

  solAssert(!ReturnBlocks.empty(), "IeleCompiler: return jmp destination not set");
  
  if (!returnExpr) {
    connectWithUnconditionalJump(CompilingBlock, ReturnBlocks.top());    
    return false;
  }

  // Visit return expression.
  llvm::SmallVector<IeleRValue *, 4> Values;
  compileTuple(*returnExpr, Values);

  TypePointer sourceType = returnExpr->annotation().type;
  TypePointers LHSTypes = FunctionType(*CompilingFunctionASTNode).returnParameterTypes();
  
  TypePointers RHSTypes;
  if (const TupleType *tupleType = dynamic_cast<const TupleType *>(sourceType)) {
    RHSTypes = tupleType->components();
  } else {
    RHSTypes = TypePointers{sourceType};
  }

  solAssert(Values.size() == RHSTypes.size(),
            "IeleCompiler: Missing value in tuple in return expression");

  solAssert(LHSTypes.size() == RHSTypes.size(),
            "IeleCompiler: Missing value in tuple in return expression");

  for (unsigned i = 0; i < Values.size(); ++i) {
    IeleLValue *LHSValue = CompilingFunctionReturnParameters[i];
    TypePointer LHSType = LHSTypes[i];
    TypePointer RHSType = RHSTypes[i];
    IeleRValue *RHSValue = Values[i];
    RHSValue = appendTypeConversion(RHSValue, RHSType, LHSType);

    // Assign to RHS.
    RHSType = RHSType->mobileType();
    if (shouldCopyStorageToStorage(*LHSType, LHSValue, *RHSType))
      appendCopyFromStorageToStorage(LHSValue, LHSType, RHSValue, RHSType);
    else if (shouldCopyMemoryToStorage(*LHSType, LHSValue, *RHSType))
      appendCopyFromMemoryToStorage(LHSValue, LHSType, RHSValue, RHSType);
    else if (shouldCopyMemoryToMemory(*LHSType, LHSValue, *RHSType))
      appendCopyFromMemoryToMemory(LHSValue, LHSType, RHSValue, RHSType);
    else
      LHSValue->write(RHSValue, CompilingBlock);
  }

  connectWithUnconditionalJump(CompilingBlock, ReturnBlocks.top());

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
    if (dynamic_cast<ContractDefinition const*>(modifierInvocation->name().annotation().referencedDeclaration)) {
      appendModifierOrFunctionCode();
    }
    else {
      // Retrieve modifier definition from its name
      solAssert(modifierInvocation->name().path().size() == 1,
                "IeleCompiler: Found modifier with compound name");
      const ASTString &modName = modifierInvocation->name().path().at(0);
      const ModifierDefinition *modifier = nullptr;
      if (contractFor(CompilingFunctionASTNode)->isLibrary()) {
        for (ModifierDefinition const* mod: contractFor(CompilingFunctionASTNode)->functionModifiers())
          if (mod->name() == modName)
            modifier = mod;
      } else {
        modifier = functionModifier(modName);
      }
      solAssert(modifier, "Could not find modifier");

      // Visit the modifier's parameters
      for (const ASTPointer<const VariableDeclaration> arg : modifier->parameters()) {
        std::vector<iele::IeleLocalVariable *> param;
        for (unsigned i = 0; i < arg->type()->sizeInRegisters(); i++) {
          std::string genName = arg->name() + getNextVarSuffix();
          auto reg = iele::IeleLocalVariable::Create(&Context, genName, CompilingFunction);
          param.push_back(reg);
        }
        IeleLValue *lvalue = RegisterLValue::Create(param);
        VarNameMap[ModifierDepth][arg->name()] = lvalue;
      }

      // Visit and initialize the modifier's local variables
      for (const VariableDeclaration *local: modifier->localVariables()) {
        std::string localName = getIeleNameForLocalVariable(local);
        std::vector<iele::IeleLocalVariable *> param;
        for (unsigned i = 0; i < local->type()->sizeInRegisters(); i++) {
          std::string genName = localName + getNextVarSuffix();
          iele::IeleLocalVariable *Local =
            iele::IeleLocalVariable::Create(&Context, genName, CompilingFunction);
          param.push_back(Local);
        }
        IeleLValue *lvalue = RegisterLValue::Create(param);
        VarNameMap[ModifierDepth][localName] = lvalue;
        appendLocalVariableInitialization(lvalue, local);
      }

      std::vector<ASTPointer<Expression>> const& modifierArguments =
        modifierInvocation->arguments() ? *modifierInvocation->arguments() : std::vector<ASTPointer<Expression>>();
      // Is the modifier invocation well formed?
      solAssert(modifier->parameters().size() == modifierArguments.size(),
             "IeleCompiler: modifier has wrong number of parameters!");

      // Get Symbol Table
      iele::IeleValueSymbolTable *ST = CompilingFunction->getIeleValueSymbolTable();
      solAssert(ST,
            "IeleCompiler: failed to access compiling function's symbol "
            "table while processing function modifer. ");

      // Cycle through each parameter-argument pair; for each one, make an assignment.
      // This way, we pass arguments into the modifier.
      for (unsigned i = 0; i < modifier->parameters().size(); ++i) {
        // Extract LHS and RHS from modifier definition and invocation
        VariableDeclaration const& var = *modifier->parameters()[i];
        Expression const& initValue    = *modifierArguments[i];

        // Temporarily set ModiferDepth to the level where all "top-level" (i.e. non-modifer related)
        // variable names are found; then, evaluate the RHS in this context;
        unsigned ModifierDepthCache = ModifierDepth;
        ModifierDepth = CompilingFunctionASTNode->modifiers().size();
        // Compile RHS expression
        IeleRValue* RHSValue = compileExpression(initValue);
        // Restore ModiferDepth to its original value
        ModifierDepth = ModifierDepthCache;

        // Lookup LHS in compiling function's variable name map.
        IeleLValue *LHSValue = VarNameMap[ModifierDepth][var.name()];
        solAssert(LHSValue, "IeleCompiler: Failed to compile argument to modifier invocation");

        // Make assignment
        LHSValue->write(RHSValue, CompilingBlock);
      }

      // Arguments to the modifier have been taken care off. Now move to modifier's body.
      codeBlock = &modifier->body();
    }
  }

  // Visit whatever is next (modifier's body or function body)
  if (codeBlock) {
    iele::IeleBlock *JumpTarget;
    if (ModifierDepth != 0) {
      JumpTarget = iele::IeleBlock::Create(
        &Context, "ret_jmp_dest");
      ReturnBlocks.push(JumpTarget);
    }
    
    codeBlock->accept(*this);

    if (ModifierDepth != 0) {
      JumpTarget -> insertInto(CompilingFunction);
      CompilingBlock = JumpTarget;
      ReturnBlocks.pop();
    }

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
      compileExpression(whileStatement.condition())->getValue();
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
      compileExpression(whileStatement.condition())->getValue();
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
      compileExpression(*forStatement.condition())->getValue();
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
    llvm::SmallVector<IeleRValue*, 4> RHSValues;
    compileTuple(*rhsExpr, RHSValues);

    // Get RHS types
    TypePointers RHSTypes;
    if (const TupleType *tupleType =
          dynamic_cast<const TupleType *>(rhsExpr->annotation().type))
      RHSTypes = tupleType->components();
    else
      RHSTypes = TypePointers{rhsExpr->annotation().type};

    // Visit assignments.
    auto const &declarations =
      variableDeclarationStatement.declarations();
    solAssert(declarations.size() == RHSValues.size(),
           "IeleCompiler: Missing assignment in variable declaration "
           "statement");
    iele::IeleValueSymbolTable *ST =
      CompilingFunction->getIeleValueSymbolTable();
    solAssert(ST,
          "IeleCompiler: failed to access compiling function's symbol "
          "table.");
    for (unsigned i = 0; i < declarations.size(); ++i) {
      const VariableDeclaration *varDecl = declarations[i].get();
      if (varDecl) {
        // Visit LHS. We lookup the LHS name in the compiling function's
        // variable name map, where we should find it.
        std::string varDeclName = getIeleNameForLocalVariable(varDecl);
        IeleLValue *LHSValue =
          VarNameMap[ModifierDepth][varDeclName];
        solAssert(LHSValue, "IeleCompiler: Failed to compile LHS of variable "
                           "declaration statement");
        // Check if we need to do a storage to memory copy.
        TypePointer LHSType = varDecl->annotation().type;
        TypePointer RHSType = RHSTypes[i];
        IeleRValue *RHSValue = RHSValues[i];
        RHSValue = appendTypeConversion(RHSValue, RHSType, LHSType);

        // Assign to RHS.
        RHSType = RHSType->mobileType();
        if (shouldCopyStorageToStorage(*LHSType, LHSValue, *RHSType))
          appendCopyFromStorageToStorage(LHSValue, LHSType, RHSValue, RHSType);
        else if (shouldCopyMemoryToStorage(*LHSType, LHSValue, *RHSType))
          appendCopyFromMemoryToStorage(LHSValue, LHSType, RHSValue, RHSType);
        else if (shouldCopyMemoryToMemory(*LHSType, LHSValue, *RHSType))
          appendCopyFromMemoryToMemory(LHSValue, LHSType, RHSValue, RHSType);
        else
          LHSValue->write(RHSValue, CompilingBlock);
      }
    }
  } else {
    // In this case we need to reassign zero to the local variable.
    solAssert(variableDeclarationStatement.declarations().size() == 1,
              "IeleCompiler: Tuple variable declaration statement without RHS "
              "was found");
    const VariableDeclaration &varDecl =
      *variableDeclarationStatement.declarations()[0];
    // We lookup the variable's name in the compiling function's
    // variable name map, where we should find it.
    std::string varDeclName = getIeleNameForLocalVariable(&varDecl);
    IeleLValue *LHSValue =
      VarNameMap[ModifierDepth][varDeclName];
    solAssert(LHSValue, "IeleCompiler: Failed to compile LHS of variable "
                       "declaration statement");
    // Zero out the variable.
    TypePointer LHSType = varDecl.annotation().type;
    appendLValueDelete(LHSValue, LHSType);
  }

  return false;
}

bool IeleCompiler::visit(const ExpressionStatement &expressionStatement) {
  llvm::SmallVector<IeleRValue*, 4> Values;
  compileTuple(expressionStatement.expression(), Values);
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
  iele::IeleValue *ConditionValue = compileExpression(condition.condition())->getValue();
  solAssert(ConditionValue, "IeleCompiler: failed to compile conditional condition.");

  llvm::SmallVector<IeleLValue *, 4> Results;
  appendConditional(ConditionValue, Results,
    [&condition, this](llvm::SmallVectorImpl<IeleRValue *> &Results){
      appendConditionalBranch(Results, condition.trueExpression(), condition.annotation().type);
    },
    [&condition, this](llvm::SmallVectorImpl<IeleRValue *> &Results){
      appendConditionalBranch(Results, condition.falseExpression(), condition.annotation().type);
    });
  CompilingExpressionResult.insert(
    CompilingExpressionResult.end(), Results.begin(), Results.end());
  return false;
}

void IeleCompiler::appendConditionalBranch(
  llvm::SmallVectorImpl<IeleRValue *> &Results,
  const Expression &Expression,
  TypePointer Type) {

  llvm::SmallVector<IeleRValue *, 4> RHSValues;
  compileTuple(Expression, RHSValues);

  TypePointers LHSTypes;
  if (const TupleType *tupleType = dynamic_cast<const TupleType *>(Type)) {
    LHSTypes = tupleType->components();
  } else {
    LHSTypes = TypePointers{Type};
  }

  appendTypeConversions(Results, RHSValues, Expression.annotation().type, LHSTypes);
}

void IeleCompiler::appendConditional(
  iele::IeleValue *ConditionValue,
  llvm::SmallVectorImpl<IeleLValue *> &ResultValues,
  const std::function<void(llvm::SmallVectorImpl<IeleRValue *> &)> &TrueExpression,
  const std::function<void(llvm::SmallVectorImpl<IeleRValue *> &)> &FalseExpression) {
  // The condition target block is the if-true block.
  iele::IeleBlock *CondTargetBlock =
    iele::IeleBlock::Create(&Context, "if.true");

  // Connect the condition block with a conditional jump to the condition target
  // block.
  connectWithConditionalJump(ConditionValue, CompilingBlock, CondTargetBlock);

  // Append the expression for the if-false block and assign it to the result.
  llvm::SmallVector<IeleRValue *, 4> FalseValues;
  FalseExpression(FalseValues);

  for (IeleRValue *FalseValue : FalseValues) {
    std::vector<iele::IeleLocalVariable *> ResultValue;
    for (unsigned i = 0; i < FalseValue->getValues().size(); i++) {
      ResultValue.push_back(iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction));
    }
    IeleLValue *lvalue = RegisterLValue::Create(ResultValue);
    lvalue->write(FalseValue, CompilingBlock);
    ResultValues.push_back(lvalue);
  }

  iele::IeleBlock *IfTrueBlock = CondTargetBlock;

  // Since we have an if-false block, we need a new join block to jump to.
  iele::IeleBlock *JoinBlock = iele::IeleBlock::Create(&Context, "if.end");

  connectWithUnconditionalJump(CompilingBlock, JoinBlock);

  // Add the if-true block at the end of the function and generate its code.
  IfTrueBlock->insertInto(CompilingFunction);
  CompilingBlock = IfTrueBlock;

  // Append the expression for the if-true block and assign it to the result.
  llvm::SmallVector<IeleRValue *, 4> TrueValues;
  TrueExpression(TrueValues);
  solAssert(TrueValues.size() == FalseValues.size(), "mismatch in number of result values of conditional");

  for (unsigned i = 0; i < TrueValues.size(); i++) {
    IeleRValue *TrueValue = TrueValues[i];
    IeleLValue *ResultValue = ResultValues[i];
    ResultValue->write(TrueValue, CompilingBlock);
  }

  // Add the join block at the end of the function and compilation continues in

  JoinBlock->insertInto(CompilingFunction);
  CompilingBlock = JoinBlock;
}

static bool isLocalVariable(const Identifier *Id) {
  if (const VariableDeclaration *Decl =
        dynamic_cast<const VariableDeclaration *>(
            Id->annotation().referencedDeclaration)) {
    return Decl->isLocalVariable();
  }

  return false;
}

// Helper functions to collect the component types and expressions of nested
// tuples. The first argument corresponds to the list of types/expressions of
// the tuple to expand. The list of "flattened" types/expressions of all, if
// any, contained nested tuples is returned in FinalComponents.
static void getTupleComponents(
    const std::vector<TypePointer> &Components,
    std::vector<TypePointer> &FinalComponents) {

  std::vector<TypePointer> ComponentsWL(Components);
  std::reverse(ComponentsWL.begin(), ComponentsWL.end());

  while (ComponentsWL.size() > 0) {
    TypePointer CType = ComponentsWL.back();
    if (CType && CType->category() == Type::Category::Tuple) {
      if (const TupleType *NestedTupleType = dynamic_cast<const TupleType *>(CType)) {
        auto NestedComponents = NestedTupleType->components();
        ComponentsWL.pop_back();
        for (auto rit = NestedComponents.crbegin() ; rit != NestedComponents.crend(); ++rit)
          ComponentsWL.push_back(*rit);
      }
    } else {
      FinalComponents.push_back(CType);
      ComponentsWL.pop_back();
    }
  }
}

static void getTupleComponents(
    const std::vector<ASTPointer<Expression>> &Components,
    std::vector<ASTPointer<Expression>> &FinalComponents) {

  std::vector<ASTPointer<Expression>> ComponentsWL(Components);
  std::reverse(ComponentsWL.begin(), ComponentsWL.end());

  while (ComponentsWL.size() > 0) {
    if (ComponentsWL.back() &&
        ComponentsWL.back()->annotation().type->category() == Type::Category::Tuple) {
      if (TupleExpression *NestedTuple = dynamic_cast<TupleExpression *>(&*ComponentsWL.back())) {
        auto NestedComponents = NestedTuple->components();
        ComponentsWL.pop_back();
        for (auto rit = NestedComponents.crbegin() ; rit != NestedComponents.crend(); ++rit)
          ComponentsWL.push_back(*rit);
      }
    } else {
      FinalComponents.push_back(ComponentsWL.back());
      ComponentsWL.pop_back();
    }
  }
}

bool IeleCompiler::visit(const Assignment &assignment) {
  Token op = assignment.assignmentOperator();
  const Expression &RHS = assignment.rightHandSide();
  const Expression &LHS = assignment.leftHandSide();

  TypePointer RHSType = RHS.annotation().type;
  TypePointer LHSType = LHS.annotation().type;

  if (RHSType->category() == Type::Category::Tuple)
    solAssert(op == Token::Assign,
              "IeleCompiler: found compound tuple assigment");

  // Collect the lhs and rhs types.
  llvm::SmallVector<TypePointer, 4> RHSTypes;
  llvm::SmallVector<TypePointer, 4> LHSTypes;
  if (RHSType->category() == Type::Category::Tuple) {
    solAssert(LHSType->category() == Type::Category::Tuple,
              "IeleCompiler: found assignment from tuple to variable");
    const TupleType &RHSTupleType = dynamic_cast<const TupleType &>(*RHSType);
    const TupleType &LHSTupleType = dynamic_cast<const TupleType &>(*LHSType);

    std::vector<TypePointer> RHSFinalComponents;
    getTupleComponents(RHSTupleType.components(), RHSFinalComponents);
    RHSTypes.insert(RHSTypes.end(), RHSFinalComponents.begin(),
                    RHSFinalComponents.end());

    std::vector<TypePointer> LHSFinalComponents;
    getTupleComponents(LHSTupleType.components(), LHSFinalComponents);
    LHSTypes.insert(LHSTypes.end(), LHSFinalComponents.begin(),
                    LHSFinalComponents.end());
  } else {
    solAssert(LHSType->category() != Type::Category::Tuple,
              "IeleCompiler: found assignment from variable to tuple");
    RHSTypes.push_back(RHSType);
    LHSTypes.push_back(LHSType);
  }

  // Visit RHS.
  llvm::SmallVector<IeleRValue *, 4> RHSValues;
  compileTuple(RHS, RHSValues);

  // Save RHS elements that are named variables (locals and formal/return
  // aguments) into temporaries to ensure correct behavior of tuple assignment.
  // For example (a, b) = (b, a) should swap the contents of a and b, and hence
  // is not equivalent to either a = b; b = a or b = a; a = b but rather to
  // t1 = b; t2 = a; b = t2; a = t1.
  if (RHSType->category() == Type::Category::Tuple) {
    if (const TupleExpression *RHSTuple =
          dynamic_cast<const TupleExpression *>(&RHS)) {
      std::vector<ASTPointer<Expression>> FinalComponents;
      getTupleComponents(RHSTuple->components(), FinalComponents);
      const auto &Components = FinalComponents;

      // Here, we also check the case of (x,) in the rhs of the assignment. We
      // need to extend RHSValues in this case to maintain the invariant that
      // LHSValues.size() <= RHSValues.size().
      if (Components.size() == 2 && RHSValues.size() == 1) {
        RHSValues.push_back(nullptr);
        RHSTypes.push_back(nullptr);
      }

      solAssert(Components.size() == RHSValues.size(),
                "IeleCompiler: failed to compile all elements of rhs of "
                "tuple assignement");
      for (unsigned i = 0; i < RHSValues.size(); ++i) {
        if (const Identifier *Id =
              dynamic_cast<const Identifier *>(&*Components[i])) {
          if (isLocalVariable(Id)) {
            std::vector<iele::IeleLocalVariable *> TmpVar;
            for (unsigned j = 0; j < RHSTypes[i]->sizeInRegisters(); j++) {
              TmpVar.push_back(iele::IeleLocalVariable::Create(
                  &Context, "tmp", CompilingFunction));
            }
            IeleLValue *TmpLValue = RegisterLValue::Create(TmpVar);
            TmpLValue->write(RHSValues[i], CompilingBlock);
            RHSValues[i] = TmpLValue->read(CompilingBlock);
          }
        }
      }
    }
  }

  // Visit LHS.
  llvm::SmallVector<IeleLValue *, 4> LHSValues;
  compileLValues(LHS, LHSValues);

  // At this point the following invariants should be true:
  solAssert(LHSValues.size() <= RHSValues.size(),
            "IeleCompiler: found tuple assignment with incompatible lhs/rhs "
            "sizes.");

  solAssert(RHSValues.size() == RHSTypes.size(),
            "IeleCompiler: found tuple with different number of components and "
            "component types.");
  solAssert(LHSValues.size() == LHSTypes.size(),
            "IeleCompiler: found tuple with different number of components and "
            "component types.");

  // Extend LHSValues if needed to be equal in size with RHSValues. Also
  // extend LHSTypes similarly.
  if (LHSValues.size() < RHSValues.size()) {
    if (LHSValues[LHSValues.size() - 1] == nullptr) {
      for (unsigned i = 0; i < RHSValues.size() - LHSValues.size(); ++i) {
        LHSValues.push_back(nullptr);
        LHSTypes.push_back(nullptr);
      }
    } else if (LHSValues[0] == nullptr) {
      for (unsigned i = 0; i < RHSValues.size() - LHSValues.size(); ++i) {
        LHSValues.insert(LHSValues.begin(), nullptr);
        LHSTypes.insert(LHSTypes.begin(), nullptr);
      }
    } else
      solAssert(false, "IeleCompiler: found tuple assignment with incompatible "
                       "lhs/rhs sizes.");
  }

  // Generate code for the assignements. To be backward compatible we need to
  // do the assignments in reverse order, since solidity-to-evm compiles this to
  // computation that pushes the rhs results into a stack and then pops and
  // assigns them one by one.
  for (int i = LHSValues.size() - 1; i >= 0; --i) {
    IeleLValue *LHSValue = LHSValues[i];
    TypePointer LHSComponentType = LHSTypes[i];
    if (!LHSValue)
      continue;

    IeleRValue *RHSValue = RHSValues[i];
    TypePointer RHSComponentType = RHSTypes[i];
    solAssert(RHSValue, "IeleCompiler: Failed to compile RHS of assignement");

    RHSValue =
      appendTypeConversion(RHSValue, RHSComponentType, LHSComponentType);
    RHSValues[i] = RHSValue; // save the converted rhs for returning as part
                             // of the assignement expression result

    // Check if we need to do a memory/storage to storage copy. Only happens when
    // assigning to a state variable of refernece type.
    RHSComponentType = RHSComponentType->mobileType();
    if (shouldCopyStorageToStorage(*LHSComponentType, LHSValue, *RHSComponentType))
      appendCopyFromStorageToStorage(LHSValue, LHSComponentType, RHSValue, RHSComponentType);
    else if (shouldCopyMemoryToStorage(*LHSComponentType, LHSValue, *RHSComponentType))
      appendCopyFromMemoryToStorage(LHSValue, LHSComponentType, RHSValue, RHSComponentType);
    else if (shouldCopyMemoryToMemory(*LHSComponentType, LHSValue, *RHSComponentType))
      appendCopyFromMemoryToMemory(LHSValue, LHSComponentType, RHSValue, RHSComponentType);
    else {
      // Check for compound assignment.
      if (op != Token::Assign) {
        Token binOp = TokenTraits::AssignmentToBinaryOp(op);
        iele::IeleValue *LHSDeref = LHSValue->read(CompilingBlock)->getValue();
        RHSValue = IeleRValue::Create(appendBinaryOperator(binOp, LHSDeref, RHSValue->getValue(), LHSComponentType));
        // save the rhs for return as the assignment expression result
        RHSValues[i] = RHSValue;
      }

      // Generate assignment code.
      LHSValue->write(RHSValue, CompilingBlock);
    }
  }

  // The result of the expression is the RHS.
  CompilingExpressionResult.insert(
    CompilingExpressionResult.end(), RHSValues.begin(), RHSValues.end());
  return false;
}

bool IeleCompiler::visit(const TupleExpression &tuple) {
  auto const &Components = tuple.components();

  // Case of inline arrays.
  if (tuple.isInlineArray()) {
    const ArrayType &arrayType =
      dynamic_cast<const ArrayType &>(*tuple.annotation().type);
    TypePointer elementType = arrayType.baseType();
    bigint elementSize = elementType->memorySize();

    // Allocate memory for the array.
    iele::IeleValue *ArrayValue = appendArrayAllocation(arrayType);

    // Visit tuple components and initialize array elements.
    bigint offset = 0;
    for (unsigned i = 0; i < Components.size(); ++i) {
      // Visit component.
      IeleRValue *InitValue = compileExpression(*Components[i]);
      TypePointer componentType = Components[i]->annotation().type;
      InitValue = appendTypeConversion(InitValue, componentType, elementType);

      // Compute lvalue in the new array in which we should assign the component
      // value.
      iele::IeleLocalVariable *AddressValue =
        iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
      iele::IeleInstruction::CreateBinOp(
          iele::IeleInstruction::Add, AddressValue, ArrayValue,
          iele::IeleIntConstant::Create(&Context, offset), CompilingBlock);
      IeleLValue *ElementLValue =
        makeLValue(AddressValue, elementType, DataLocation::Memory);

      // Do the assignment.
      componentType = componentType->mobileType();
      solAssert(!shouldCopyStorageToStorage(
                    *elementType, ElementLValue, *componentType),
                "IeleCompiler: storage to storage copy needed in "
                "initialization of inline array.");
      solAssert(!shouldCopyMemoryToStorage(
                    *elementType, ElementLValue, *componentType),
                "IeleCompiler: memory to storage copy needed in "
                "initialization of inline array.");
      if (shouldCopyMemoryToMemory(
              *elementType, ElementLValue, *componentType)) {
        appendCopyFromMemoryToMemory(
            ElementLValue, elementType, InitValue, componentType);
      } else {
        IeleLValue *lvalue =
           AddressLValue::Create(this, AddressValue, DataLocation::Memory,
                                 "loaded", elementType->sizeInRegisters());
        lvalue->write(InitValue, CompilingBlock);
      }

      offset += elementSize;
    }

    // Return pointer to the newly allocated array.
    CompilingExpressionResult.push_back(IeleRValue::Create(ArrayValue));
    return false;
  }

  // Case of tuple.
  llvm::SmallVector<Value, 4> Results;
  for (unsigned i = 0; i < Components.size(); i++) {
    const ASTPointer<Expression> &component = Components[i];
    if (CompilingLValue && component) {
      llvm::SmallVector<IeleLValue *, 4> LVals;
      compileLValues(*component, LVals);
      Results.insert(Results.end(), LVals.begin(), LVals.end());
    } else if (CompilingLValue) {
      Results.push_back((IeleLValue *)nullptr);
    } else if (component) {
      llvm::SmallVector<IeleRValue *, 4> RVals;
      compileTuple(*component, RVals);
      Results.insert(Results.end(), RVals.begin(), RVals.end());
    } else {
      // this the special (x,) rvalue tuple, the only case where an rvalue tuple
      // is allowed to skip a component.
      solAssert(Components.size() == 2 && i == 1,
                "IeleCompiler: found rvalue tuple with missing elements");
    }
  }

  CompilingExpressionResult.insert(
    CompilingExpressionResult.end(), Results.begin(), Results.end());

  return false;
}

bool IeleCompiler::visit(const UnaryOperation &unaryOperation) {
  // First check for constants.
  TypePointer ResultType = unaryOperation.annotation().type;
  if (ResultType->category() == Type::Category::RationalNumber) {
    iele::IeleIntConstant *LiteralValue =
      iele::IeleIntConstant::Create(
          &Context, 
          ResultType->literalValue(nullptr));
    CompilingExpressionResult.push_back(IeleRValue::Create(LiteralValue));
    return false;
  }

  Token UnOperator = unaryOperation.getOperator();
  switch (UnOperator) {
  case Token::Not: {// !
    // Visit subexpression.
    iele::IeleValue *SubExprValue =
      compileExpression(unaryOperation.subExpression())->getValue();
    solAssert(SubExprValue, "IeleCompiler: Failed to compile operand.");
    // Compile as an iszero.
    iele::IeleLocalVariable *Result =
      iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
    iele::IeleInstruction::CreateIsZero(Result, SubExprValue, CompilingBlock);
    CompilingExpressionResult.push_back(IeleRValue::Create(Result));
    break;
  }
  case Token::Add: { // +
    // Visit subexpression.
    iele::IeleValue *SubExprValue =
      compileExpression(unaryOperation.subExpression())->getValue();
    solAssert(SubExprValue, "IeleCompiler: Failed to compile operand.");
    // unary add, so basically no-op
    CompilingExpressionResult.push_back(IeleRValue::Create(SubExprValue));
    break;
  }
  case Token::Sub: { // -
    // Visit subexpression.
    iele::IeleValue *SubExprValue =
      compileExpression(unaryOperation.subExpression())->getValue();
    solAssert(SubExprValue, "IeleCompiler: Failed to compile operand.");
    // Compile as a subtraction from zero.
    iele::IeleIntConstant *Zero = iele::IeleIntConstant::getZero(&Context);
    iele::IeleLocalVariable *Result =
      appendBinaryOperator(Token::Sub, Zero, SubExprValue, ResultType);
    CompilingExpressionResult.push_back(IeleRValue::Create(Result));
    break;
  }
  case Token::Inc:    // ++ (pre- or postfix)
  case Token::Dec:  { // -- (pre- or postfix)
    Token BinOperator =
      (UnOperator == Token::Inc) ? Token::Add : Token::Sub;
    // Compile subexpression as an lvalue.
    IeleLValue *SubExprValue =
      compileLValue(unaryOperation.subExpression());
    solAssert(SubExprValue, "IeleCompiler: Failed to compile operand.");
    // Get the current subexpression value, and save it in case of a postfix
    // oparation.
    iele::IeleValue *Before = SubExprValue->read(CompilingBlock)->getValue();
    // Generate code for the inc/dec operation.
    iele::IeleIntConstant *One = iele::IeleIntConstant::getOne(&Context);
    iele::IeleLocalVariable *After =
      appendBinaryOperator(BinOperator, Before, One, ResultType);
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
    SubExprValue->write(IeleRValue::Create(After), CompilingBlock);
    // Return result.
    CompilingExpressionResult.push_back(IeleRValue::Create(Result));
    break;
  }
  case Token::BitNot: { // ~
    // Visit subexpression.
    iele::IeleValue *SubExprValue =
      compileExpression(unaryOperation.subExpression())->getValue();
    solAssert(SubExprValue, "IeleCompiler: Failed to compile operand.");

    bool fixed = false, issigned = false;
    int nbytes;
    switch (ResultType->category()) {
    case Type::Category::Integer: {
      const IntegerType *type = dynamic_cast<const IntegerType *>(ResultType);
      fixed = !type->isUnbound();
      nbytes = fixed ? type->numBits() / 8 : 0;
      issigned = type->isSigned();
      break;
    }
    case Type::Category::FixedBytes: {
      const FixedBytesType *type = dynamic_cast<const FixedBytesType *>(ResultType);
      fixed = true;
      nbytes = type->numBytes();
      break;
    }
    default: break;
    }

    solAssert(fixed || issigned,
              "IeleCompiler: found bitwise negation on unbound unsigned "
              "integer");

    // Compile as a bitwise negation.
    iele::IeleLocalVariable *Result =
      iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
    iele::IeleInstruction::CreateNot(Result, SubExprValue, CompilingBlock);

    // Mask in case of fixed-size operation.
    if (fixed && !issigned) {
      appendMask(Result, Result, nbytes, false);
    }

    CompilingExpressionResult.push_back(IeleRValue::Create(Result));
    break;
  }
  case Token::Delete: { // delete
    llvm::SmallVector<IeleLValue *, 4> LValues;
    compileLValues(unaryOperation.subExpression(), LValues);

    solAssert(LValues.size() == 1, "not implemented yet");
    TypePointer type = unaryOperation.subExpression().annotation().type;
    appendLValueDelete(LValues[0], type);
    break;
  }
  default:
    solAssert(false, "IeleCompiler: Invalid unary operator");
    break;
  }

  return false;
}

IeleLValue *IeleCompiler::makeLValue(
    iele::IeleValue *Address, TypePointer type, DataLocation Loc,
    iele::IeleValue *Offset) {
  if (Offset) {
    return ByteArrayLValue::Create(this, Address, Offset, Loc);
  }

  if (type->isValueType() ||
      (type->isDynamicallySized() &&
       (Loc == DataLocation::Memory || Loc == DataLocation::CallData))) {
    if (auto arrayType = dynamic_cast<const ArrayType *>(type)) {
      if (arrayType->isByteArray()) {
        return ReadOnlyLValue::Create(IeleRValue::Create(Address));
      }
    }
    return AddressLValue::Create(this, Address, Loc, "loaded",
                                 type->sizeInRegisters());
  }

  if (!llvm::isa<iele::IeleGlobalVariable>(Address) &&
      type->category() == Type::Category::Struct &&
      Loc == DataLocation::Storage) {
    const StructType &structType = dynamic_cast<const StructType &>(*type);
    if (structType.recursive()) {
      return RecursiveStructLValue::Create(
                 IeleRValue::Create(Address), structType, this);
    }
  }

  return ReadOnlyLValue::Create(IeleRValue::Create(Address));
}

void IeleCompiler::appendLValueDelete(IeleLValue *LValue, TypePointer type) {
  if (type->isValueType()) {
    LValue->write(IeleRValue::CreateZero(&Context, type->sizeInRegisters()), CompilingBlock);
    return;
  }
  iele::IeleValue *Address = LValue->read(CompilingBlock)->getValue();
  if (!type->isDynamicallyEncoded()) {
    iele::IeleValue *SizeValue = getReferenceTypeSize(*type, Address);
    if (type->dataStoredIn(DataLocation::Storage)) {
      appendIeleRuntimeFill(Address, SizeValue, 
        iele::IeleIntConstant::getZero(&Context),
        DataLocation::Storage);
    } else {
      solAssert(type->dataStoredIn(DataLocation::Memory), "reference type should be in memory or storage");
      appendIeleRuntimeFill(Address, SizeValue, 
        iele::IeleIntConstant::getZero(&Context),
        DataLocation::Memory);
    }
    return;
  }
  switch (type->category()) {
  case Type::Category::Struct: {
    const StructType &structType = dynamic_cast<const StructType &>(*type);

    if (structType.recursive()) {
      // Call the recursive destructor.
      iele::IeleFunction *Destructor = getRecursiveStructDestructor(structType);
      llvm::SmallVector<iele::IeleLocalVariable *, 0> EmptyResults;
      llvm::SmallVector<iele::IeleValue *, 1> Arguments(1, Address);
      iele::IeleInstruction::CreateInternalCall(
          EmptyResults, Destructor, Arguments, CompilingBlock);
      break;
    }

    // Append the code for deleting the struct members.
    appendStructDelete(structType, Address);
    break;
  }
  case Type::Category::Array: {
    const ArrayType &arrayType = dynamic_cast<const ArrayType &>(*type);
    const Type &elementType = *arrayType.baseType();
    if (!elementType.isDynamicallyEncoded()) {
      iele::IeleValue *SizeValue = getReferenceTypeSize(*type, Address);
        appendIeleRuntimeFill(Address, SizeValue, 
          iele::IeleIntConstant::getZero(&Context),
          arrayType.location());
      return;
    }

    iele::IeleLocalVariable *SizeVariable =
      iele::IeleLocalVariable::Create(&Context, "array.size", CompilingFunction);
    iele::IeleLocalVariable *Element =
      iele::IeleLocalVariable::Create(&Context, "fill.address", CompilingFunction);
    if (type->isDynamicallySized()) {
      arrayType.location() == DataLocation::Storage ?
        iele::IeleInstruction::CreateSLoad(
          SizeVariable, Address,
          CompilingBlock) :
        iele::IeleInstruction::CreateLoad(
          SizeVariable, Address,
          CompilingBlock) ;

      arrayType.location() == DataLocation::Storage ?
        iele::IeleInstruction::CreateSStore(
          iele::IeleIntConstant::getZero(&Context), Address,
          CompilingBlock) :
        iele::IeleInstruction::CreateStore(
          iele::IeleIntConstant::getZero(&Context), Address,
          CompilingBlock) ;

      iele::IeleInstruction::CreateBinOp(
          iele::IeleInstruction::Add, Element, Address,
          iele::IeleIntConstant::getOne(&Context),
          CompilingBlock);
    } else {
      iele::IeleInstruction::CreateAssign(
        SizeVariable, iele::IeleIntConstant::Create(&Context, bigint(arrayType.length())),
        CompilingBlock);
      iele::IeleInstruction::CreateAssign(
        Element, Address, CompilingBlock);
    }

    appendArrayDeleteLoop(arrayType, Element, SizeVariable);
    break;
  }
  case Type::Category::Mapping:
    // Delete on whole mappings is a noop in Solidity.
    break;
  default:
    solAssert(false, "not implemented yet");
  }
}

iele::IeleFunction *IeleCompiler::getRecursiveStructDestructor(
    const StructType &type) {
  solAssert(type.recursive(),
            "IeleCompiler: attempted to construct recursive destructor "
            "for non-recursive struct type");

  std::string typeID = type.richIdentifier();

  // Lookup destructor in the cache.
  auto It = RecursiveStructDestructors.find(typeID);
  if (It != RecursiveStructDestructors.end())
    return It->second;

  // Generate a recursive function to delete a struct of the specific type.
  iele::IeleFunction *Destructor =
    iele::IeleFunction::Create(&Context, false, "ielert.delete." + typeID,
                               CompilingContract);

  // Register destructor function.
  RecursiveStructDestructors[typeID] = Destructor;

  // Add the address argument.
  iele::IeleArgument *Address =
    iele::IeleArgument::Create(&Context, "delete.addr", Destructor);

  // Add the first block.
  iele::IeleBlock *DestructorBlock =
    iele::IeleBlock::Create(&Context, "entry", Destructor);

  // Append the code for deleting the struct members and returning.
  std::swap(CompilingFunction, Destructor);
  std::swap(CompilingBlock, DestructorBlock);
  appendStructDelete(type, Address);
  iele::IeleInstruction::CreateRetVoid(CompilingBlock);
  std::swap(CompilingFunction, Destructor);
  std::swap(CompilingBlock, DestructorBlock);

  return Destructor;
}

void IeleCompiler::appendStructDelete(
    const StructType &type, iele::IeleValue *Address) {
  // For each member of the struct, generate code that deletes that member.
  for (const auto &member : type.members(nullptr)) {
    // First compute the offset from the start of the struct.
    bigint offset;
    switch (type.location()) {
    case DataLocation::Storage: {
      const auto& offsets = type.storageOffsetsOfMember(member.name);
      offset = offsets.first;
      break;
    }
    case DataLocation::CallData:
    case DataLocation::Memory: {
      offset = type.memoryOffsetOfMember(member.name);
      break;
    }
    }

    iele::IeleLocalVariable *Member =
      iele::IeleLocalVariable::Create(&Context, "fill.address",
                                      CompilingFunction);
    iele::IeleValue *OffsetValue =
      iele::IeleIntConstant::Create(&Context, offset);
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Add, Member, Address, OffsetValue,
      CompilingBlock);

    // Then delete member at that offset.
    appendLValueDelete(makeLValue(Member, member.type, type.location()),
                       member.type);
  }
}

void IeleCompiler::appendArrayDeleteLoop(
    const ArrayType &type, iele::IeleLocalVariable *ElementAddressVariable,
    iele::IeleLocalVariable *NumElementsVariable) {
  TypePointer elementType = type.baseType();

  iele::IeleBlock *LoopBodyBlock =
    iele::IeleBlock::Create(&Context, "delete.loop", CompilingFunction);
  CompilingBlock = LoopBodyBlock;

  iele::IeleBlock *LoopExitBlock =
    iele::IeleBlock::Create(&Context, "delete.loop.end");

  iele::IeleLocalVariable *DoneValue =
    iele::IeleLocalVariable::Create(&Context, "done", CompilingFunction);
  iele::IeleInstruction::CreateIsZero(
    DoneValue, NumElementsVariable, CompilingBlock);
  connectWithConditionalJump(DoneValue, CompilingBlock, LoopExitBlock);

  bigint elementSize;
  switch (type.location()) {
  case DataLocation::Storage: {
    elementSize = elementType->storageSize();
    break;
  }
  case DataLocation::CallData:
  case DataLocation::Memory: {
    elementSize = elementType->memorySize();
    break;
  }
  }

  appendLValueDelete(
      makeLValue(ElementAddressVariable, type.baseType(), type.location()),
      elementType);

  iele::IeleValue *ElementSizeValue =
      iele::IeleIntConstant::Create(&Context, elementSize);

  iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Add, ElementAddressVariable,
      ElementAddressVariable, ElementSizeValue, CompilingBlock);
  iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Sub, NumElementsVariable, NumElementsVariable,
      iele::IeleIntConstant::getOne(&Context), CompilingBlock);

  connectWithUnconditionalJump(CompilingBlock, LoopBodyBlock);
  LoopExitBlock->insertInto(CompilingFunction);
  CompilingBlock = LoopExitBlock;
}

bool IeleCompiler::visit(const BinaryOperation &binaryOperation) {
  // Short-circuiting cases.
  if (binaryOperation.getOperator() == Token::Or || binaryOperation.getOperator() == Token::And) {
    iele::IeleValue *Result =
      appendBooleanOperator(binaryOperation.getOperator(),
                            binaryOperation.leftExpression(),
                            binaryOperation.rightExpression());
    CompilingExpressionResult.push_back(IeleRValue::Create(Result));
    return false;
  }

  const TypePointer &commonType = binaryOperation.annotation().commonType;

  // Check for compile-time constants.
  if (commonType->category() == Type::Category::RationalNumber) {
    iele::IeleIntConstant *LiteralValue =
      iele::IeleIntConstant::Create(
        &Context,
        commonType->literalValue(nullptr));
    CompilingExpressionResult.push_back(IeleRValue::Create(LiteralValue));
    return false;
  }

  // Visit operands.
  IeleRValue *LeftOperandValue = 
    compileExpression(binaryOperation.leftExpression());
  solAssert(LeftOperandValue, "IeleCompiler: Failed to compile left operand.");
  IeleRValue *RightOperandValue = 
    compileExpression(binaryOperation.rightExpression());
  solAssert(RightOperandValue, "IeleCompiler: Failed to compile right operand.");

  LeftOperandValue = appendTypeConversion(LeftOperandValue,
    binaryOperation.leftExpression().annotation().type,
    commonType);
  if (!TokenTraits::isShiftOp(binaryOperation.getOperator()) &&
      binaryOperation.getOperator() != Token::Exp) {
    RightOperandValue = appendTypeConversion(RightOperandValue,
      binaryOperation.rightExpression().annotation().type,
      commonType);
  }
  // Append the IELE code for the binary operator.
  iele::IeleValue *Result =
    appendBinaryOperator(binaryOperation.getOperator(),
                         LeftOperandValue->getValue(), RightOperandValue->getValue(),
                         binaryOperation.annotation().type);

  CompilingExpressionResult.push_back(IeleRValue::Create(Result));
  return false;
}

/// Perform encoding of the given values, and returns a register containing it
IeleRValue *IeleCompiler::encoding(
    IeleRValue *argument,
    TypePointer type,
    bool hash) {
  if (type->isValueType() || type->category() == Type::Category::Mapping) {
    return argument;
  }

  // Allocate cell 
  iele::IeleLocalVariable *NextFree = appendMemorySpill();

  llvm::SmallVector<IeleRValue *, 1> arguments;
  arguments.push_back(argument);

  TypePointers types;
  types.push_back(type);

  // Call version of `encoding` that writes destination loc (NextFree)
  encoding(arguments, types, NextFree, !hash);

  // Init register to be returned
  iele::IeleLocalVariable *EncodedVal = 
    iele::IeleLocalVariable::Create(&Context, "encoded.val", CompilingFunction);
  if (hash) {
    iele::IeleInstruction::CreateSha3(
      EncodedVal, NextFree, CompilingBlock);
  } else {
    // Load encoding into regster to be returned
    iele::IeleInstruction::CreateLoad(EncodedVal, NextFree, CompilingBlock);
  }
  
  // Return register holding encoded bytestring
  return IeleRValue::Create(EncodedVal);
}

void IeleCompiler::appendByteWidth(iele::IeleLocalVariable *Result, iele::IeleValue *Value) {
   // Calculate width in bytes of argument:
   // width_bytes(n) = ((log2(n) + 2) + 7) / 8
   //                = (log2(n) + 9) / 8
   //                = (log2(n) + 9) >> 3

   // Check for positive value.
   iele::IeleLocalVariable *IsPositive =
     iele::IeleLocalVariable::Create(
       &Context, "is.positive", CompilingFunction);
   iele::IeleInstruction::CreateBinOp(
     iele::IeleInstruction::CmpGt, IsPositive, Value,
     iele::IeleIntConstant::getZero(&Context), CompilingBlock);

   llvm::SmallVector<IeleLValue *, 1> Results;
   appendConditional(IsPositive, Results,
    [&Value, this](llvm::SmallVectorImpl<IeleRValue *> &Results){
      // Compute the log.
      iele::IeleLocalVariable *Result =
        iele::IeleLocalVariable::Create(
          &Context, "logarithm.base.2", CompilingFunction);
      iele::IeleInstruction::CreateLog2(Result, Value, CompilingBlock);
      Results.push_back(IeleRValue::Create(Result));
    },
    [&Value, this](llvm::SmallVectorImpl<IeleRValue *> &Results){
      // Check for 0 or -1 value.
      iele::IeleLocalVariable *IsZeroOrMinusOne =
        iele::IeleLocalVariable::Create(
          &Context, "is.zero.or.minus.one", CompilingFunction);
      iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::CmpGe, IsZeroOrMinusOne, Value,
        iele::IeleIntConstant::getMinusOne(&Context), CompilingBlock);

      llvm::SmallVector<IeleLValue *, 1> NonPositiveResults;
      appendConditional(IsZeroOrMinusOne, NonPositiveResults,
       [this](llvm::SmallVectorImpl<IeleRValue *> &Results){
         // Return 0.
         Results.push_back(
           IeleRValue::Create(iele::IeleIntConstant::getZero(&Context)));
       },
       [&Value, this](llvm::SmallVectorImpl<IeleRValue *> &Results){
         // Compute the log of the negated number.
         iele::IeleLocalVariable *PositiveValue =
           iele::IeleLocalVariable::Create(
             &Context, "positive.value", CompilingFunction);
        iele::IeleInstruction::CreateNot(PositiveValue, Value, CompilingBlock);
        iele::IeleLocalVariable *Result =
          iele::IeleLocalVariable::Create(
            &Context, "logarithm.base.2", CompilingFunction);
        iele::IeleInstruction::CreateLog2(
          Result, PositiveValue, CompilingBlock);
        Results.push_back(IeleRValue::Create(Result));
       });
       solAssert(NonPositiveResults.size() == 1,
                 "Invalid number of conditional results in appendByteWidth");

      // Forward the result.
      iele::IeleValue *NonPositiveResult =
        NonPositiveResults[0]->read(CompilingBlock)->getValue();
      Results.push_back(IeleRValue::Create(NonPositiveResult));
    });
   solAssert(Results.size() == 1, "Invalid number of conditional results in appendByteWidth");
   iele::IeleInstruction::CreateBinOp(
     iele::IeleInstruction::Add, Result, 
     Results[0]->read(CompilingBlock)->getValue(),
     iele::IeleIntConstant::Create(&Context, 9), 
     CompilingBlock);
   iele::IeleInstruction::CreateBinOp(
     iele::IeleInstruction::Shift, Result, 
     Result,
     iele::IeleIntConstant::Create(&Context, bigint(-3)), 
     CompilingBlock);
}

/// Perform encoding of the given values, and writes in provided the destination
iele::IeleValue *IeleCompiler::encoding(
    llvm::SmallVectorImpl<IeleRValue *> &arguments, 
    TypePointers types,
    iele::IeleLocalVariable *NextFree,
    bool appendWidths) {
  bool bigEndian = !appendWidths;
  solAssert(arguments.size() == types.size(), "invalid number of arguments to encoding");
  if (types.size() == 1 && types[0]->isValueType()) {
    appendWidths = false;
  }

  // Create local vars needed for encoding
  iele::IeleLocalVariable *CrntPos = 
    iele::IeleLocalVariable::Create(&Context, "crnt.pos", CompilingFunction);
  iele::IeleLocalVariable *ArgTypeSize = 
    iele::IeleLocalVariable::Create(&Context, "arg.type.size", 
                                    CompilingFunction);
  iele::IeleLocalVariable *ArgLen = 
    iele::IeleLocalVariable::Create(&Context, "arg.len", CompilingFunction);
 
  iele::IeleInstruction::CreateAssign(
    CrntPos, iele::IeleIntConstant::getZero(&Context), CompilingBlock);    
  for (unsigned i = 0; i < arguments.size(); i++) {
    doEncode(NextFree, CrntPos, ReadOnlyLValue::Create(arguments[i]), ArgTypeSize, ArgLen, types[i], appendWidths, bigEndian);
  }
  return CrntPos;
}

void IeleCompiler::doEncode(
    iele::IeleValue *NextFree,
    iele::IeleLocalVariable *CrntPos, IeleLValue *LValue, 
    iele::IeleLocalVariable *ArgTypeSize, iele::IeleLocalVariable *ArgLen,
    TypePointer type, bool appendWidths, bool bigEndian) {
  IeleRValue *ArgValue = LValue->read(CompilingBlock);
  switch (type->category()) {
  case Type::Category::Contract:
  case Type::Category::Address:
  case Type::Category::FixedBytes:
  case Type::Category::Enum:
  case Type::Category::Bool:
  case Type::Category::Integer: {
    // width of given type, in bits, or 0 arbitrary precision
    bigint bitwidth = type->getFixedBitwidth();
    if (bitwidth != 0) { // Fixed-precision 
      // width of given type, in bytes
      bigint size = (bitwidth + 7) / 8;
      // codegen
      iele::IeleInstruction::CreateAssign(
        ArgTypeSize, 
        iele::IeleIntConstant::Create(&Context, size), 
        CompilingBlock);    
      iele::IeleValue *Bswapped;
      if (bigEndian) {
        Bswapped = iele::IeleLocalVariable::Create(&Context, "byte.swapped", CompilingFunction);
        iele::IeleInstruction::CreateBinOp(
          iele::IeleInstruction::BSwap, 
          llvm::dyn_cast<iele::IeleLocalVariable>(Bswapped), ArgTypeSize, ArgValue->getValue(),
          CompilingBlock);
      } else {
        Bswapped = ArgValue->getValue();
      }
      iele::IeleInstruction::CreateStore(
        Bswapped, NextFree, CrntPos, 
        ArgTypeSize, CompilingBlock);
      iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Add, CrntPos, CrntPos, 
        ArgTypeSize, CompilingBlock);
    }
    else { // Arbitrary precision
      bigint argLenWidth = bigint(8); // Store length using 8 bytes 
      // Calculate width in bytes of argument:
      // width_bytes(n) = ((log2(n) + 1) + 7) / 8
      //                = (log2(n) + 8) / 8
      appendByteWidth(ArgLen, ArgValue->getValue());
      if (appendWidths) {
        iele::IeleInstruction::CreateAssign(
          ArgTypeSize, iele::IeleIntConstant::Create(&Context, argLenWidth), 
          CompilingBlock);
        iele::IeleInstruction::CreateStore(
          ArgLen, NextFree, CrntPos, ArgTypeSize, CompilingBlock);
        iele::IeleInstruction::CreateBinOp(
          iele::IeleInstruction::Add, CrntPos, CrntPos, 
          ArgTypeSize, CompilingBlock);
      }
      iele::IeleValue *Bswapped;
      if (bigEndian) {
        Bswapped = iele::IeleLocalVariable::Create(&Context, "byte.swapped", CompilingFunction);
        iele::IeleInstruction::CreateBinOp(
          iele::IeleInstruction::BSwap, 
          llvm::dyn_cast<iele::IeleLocalVariable>(Bswapped), ArgLen, ArgValue->getValue(),
          CompilingBlock);
      } else {
        Bswapped = ArgValue->getValue();
      }
      iele::IeleInstruction::CreateStore(
        Bswapped, NextFree, CrntPos, ArgLen, CompilingBlock);
      iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Add, CrntPos, CrntPos, ArgLen, CompilingBlock);
    }
    break;
  }
  case Type::Category::Mapping:
    // we do not actually encode mappings
    break;
  case Type::Category::Array: {
    const ArrayType &arrayType = dynamic_cast<const ArrayType &>(*type);
    TypePointer elementType = arrayType.baseType();
    if (arrayType.isByteArray()) {
      bigint argLenWidth = bigint(8); // Store length using 8 bytes 
      arrayType.location() == DataLocation::Storage ?
        iele::IeleInstruction::CreateSLoad(
          ArgLen, ArgValue->getValue(),
          CompilingBlock) :
        iele::IeleInstruction::CreateLoad(
          ArgLen, ArgValue->getValue(),
          CompilingBlock) ;

      if (appendWidths) {
        iele::IeleInstruction::CreateAssign(
          ArgTypeSize, iele::IeleIntConstant::Create(&Context, argLenWidth), 
          CompilingBlock);
        iele::IeleInstruction::CreateStore(
          ArgLen, NextFree, CrntPos, ArgTypeSize, CompilingBlock);
        iele::IeleInstruction::CreateBinOp(
          iele::IeleInstruction::Add, CrntPos, CrntPos, 
          ArgTypeSize, CompilingBlock);
      }
      iele::IeleLocalVariable *StringAddress =
        iele::IeleLocalVariable::Create(&Context, "string.address", CompilingFunction);
      iele::IeleLocalVariable *StringValue =
        iele::IeleLocalVariable::Create(&Context, "string.value", CompilingFunction);
      iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Add, StringAddress, ArgValue->getValue(),
        iele::IeleIntConstant::getOne(&Context),
        CompilingBlock);
      arrayType.location() == DataLocation::Storage ?
        iele::IeleInstruction::CreateSLoad(
          StringValue, StringAddress,
          CompilingBlock) :
        iele::IeleInstruction::CreateLoad(
          StringValue, StringAddress,
          CompilingBlock) ;
      iele::IeleValue *Bswapped;
      // register is already little endian, so we need to reverse this condition
      if (!bigEndian) {
        Bswapped = iele::IeleLocalVariable::Create(&Context, "byte.swapped", CompilingFunction);
        iele::IeleInstruction::CreateBinOp(
          iele::IeleInstruction::BSwap, 
          llvm::dyn_cast<iele::IeleLocalVariable>(Bswapped), ArgLen, StringValue,
          CompilingBlock);
      } else {
        Bswapped = StringValue;
      }
      iele::IeleInstruction::CreateStore(
        Bswapped, NextFree, CrntPos, ArgLen, CompilingBlock);
      iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Add, CrntPos, CrntPos, ArgLen, CompilingBlock);
    } else {
      iele::IeleLocalVariable *ArrayLen =
        iele::IeleLocalVariable::Create(&Context, "array.length", CompilingFunction);
      iele::IeleLocalVariable *Element =
        iele::IeleLocalVariable::Create(&Context, "array.element", CompilingFunction);
      if (arrayType.isDynamicallySized()) {
        arrayType.location() == DataLocation::Storage ?
          iele::IeleInstruction::CreateSLoad(
            ArrayLen, ArgValue->getValue(),
            CompilingBlock) :
          iele::IeleInstruction::CreateLoad(
            ArrayLen, ArgValue->getValue(),
            CompilingBlock) ;
        appendByteWidth(ArgLen, ArrayLen);
        if (appendWidths) {
          iele::IeleInstruction::CreateAssign(
            ArgTypeSize, iele::IeleIntConstant::getOne(&Context), 
            CompilingBlock);    
          iele::IeleInstruction::CreateStore(
            ArgLen, NextFree, CrntPos, ArgTypeSize, CompilingBlock);
          iele::IeleInstruction::CreateBinOp(
            iele::IeleInstruction::Add, CrntPos, CrntPos, 
            ArgTypeSize, CompilingBlock);
          iele::IeleInstruction::CreateStore(
            ArrayLen, NextFree, CrntPos, ArgLen, CompilingBlock);
          iele::IeleInstruction::CreateBinOp(
            iele::IeleInstruction::Add, CrntPos, CrntPos, ArgLen, CompilingBlock);
        }
        iele::IeleInstruction::CreateBinOp(
          iele::IeleInstruction::Add,
          Element, ArgValue->getValue(), 
          iele::IeleIntConstant::getOne(&Context),
          CompilingBlock);
      } else {
        iele::IeleInstruction::CreateAssign(
          ArrayLen,
          iele::IeleIntConstant::Create(&Context, arrayType.length()),
          CompilingBlock);
        iele::IeleInstruction::CreateAssign(
          Element, ArgValue->getValue(), CompilingBlock);
      }

      iele::IeleBlock *LoopBodyBlock =
        iele::IeleBlock::Create(&Context, "encode.loop", CompilingFunction);
      CompilingBlock = LoopBodyBlock;
  
      iele::IeleBlock *LoopExitBlock =
        iele::IeleBlock::Create(&Context, "encode.loop.end");
  
      iele::IeleLocalVariable *DoneValue =
        iele::IeleLocalVariable::Create(&Context, "done", CompilingFunction);
      iele::IeleInstruction::CreateIsZero(
        DoneValue, ArrayLen, CompilingBlock);
      connectWithConditionalJump(DoneValue, CompilingBlock, LoopExitBlock);
  
      bigint elementSize;
      switch (arrayType.location()) {
      case DataLocation::Storage: {
        elementSize = elementType->storageSize();
        break;
      }
      case DataLocation::CallData:
      case DataLocation::Memory: {
        elementSize = elementType->memorySize();
        break;
      }
      }
  
      elementType = TypeProvider::withLocationIfReference(arrayType.location(), elementType);
      doEncode(NextFree, CrntPos, makeLValue(Element, elementType, arrayType.location()), ArgTypeSize, ArgLen, elementType, appendWidths, bigEndian);
  
      iele::IeleValue *ElementSizeValue =
          iele::IeleIntConstant::Create(&Context, elementSize);
  
      iele::IeleInstruction::CreateBinOp(
          iele::IeleInstruction::Add, Element, Element,
          ElementSizeValue,
          CompilingBlock);
      iele::IeleInstruction::CreateBinOp(
          iele::IeleInstruction::Sub, ArrayLen, ArrayLen,
          iele::IeleIntConstant::getOne(&Context),
          CompilingBlock);
  
      connectWithUnconditionalJump(CompilingBlock, LoopBodyBlock);
      LoopExitBlock->insertInto(CompilingFunction);
      CompilingBlock = LoopExitBlock;
    }
    break;
  }
  case Type::Category::Struct: {
    const StructType &structType = dynamic_cast<const StructType &>(*type);

    if (structType.recursive()) {
      // Call the recursive encoder.
      iele::IeleFunction *Encoder =
        getRecursiveStructEncoder(structType, appendWidths, bigEndian);
      llvm::SmallVector<iele::IeleLocalVariable *, 1> Results(1, CrntPos);
      llvm::SmallVector<iele::IeleValue *, 3> Arguments;
      Arguments.push_back(ArgValue->getValue());
      Arguments.push_back(NextFree);
      Arguments.push_back(CrntPos);
      iele::IeleInstruction::CreateInternalCall(
          Results, Encoder, Arguments, CompilingBlock);
      break;
    }

    appendStructEncode(
        structType, ArgValue->getValue(), ArgTypeSize, ArgLen, NextFree,
        CrntPos, appendWidths, bigEndian);
    break;
  }
  case Type::Category::Function: {
    const FunctionType &functionType = dynamic_cast<const FunctionType &>(*type);
    FunctionType::Kind kind;
    if (functionType.kind() == FunctionType::Kind::SetGas || functionType.kind() == FunctionType::Kind::SetValue)
    {
      solAssert(functionType.returnParameterTypes().size() == 1, "");
      kind = dynamic_cast<FunctionType const&>(*functionType.returnParameterTypes().front()).kind();
    } else {
      kind = functionType.kind();
    }
    unsigned index = 0;
    switch (kind) {
    case FunctionType::Kind::External:
      doEncode(NextFree, CrntPos, ReadOnlyLValue::Create(IeleRValue::Create(ArgValue->getValues()[0])), ArgTypeSize, ArgLen, Address, appendWidths, bigEndian);
      doEncode(NextFree, CrntPos, ReadOnlyLValue::Create(IeleRValue::Create(ArgValue->getValues()[1])), ArgTypeSize, ArgLen, UInt16, appendWidths, bigEndian);
      index = 2;
      break;
    case FunctionType::Kind::BareCall:
    case FunctionType::Kind::BareCallCode:
    case FunctionType::Kind::BareDelegateCall:
    case FunctionType::Kind::BareStaticCall:
    case FunctionType::Kind::Internal:
    case FunctionType::Kind::DelegateCall:
      doEncode(NextFree, CrntPos, ReadOnlyLValue::Create(IeleRValue::Create(ArgValue->getValues()[0])), ArgTypeSize, ArgLen, UInt16, appendWidths, bigEndian);
      index = 1;
      break;
    case FunctionType::Kind::ArrayPush:
    case FunctionType::Kind::ByteArrayPush:
    case FunctionType::Kind::ArrayPop:
      solAssert(false, "not implemented yet");
    default:
      break;
    }
    if (functionType.gasSet()) {
      doEncode(NextFree, CrntPos, ReadOnlyLValue::Create(IeleRValue::Create(ArgValue->getValues()[index])), ArgTypeSize, ArgLen, UInt, appendWidths, bigEndian);
      index++;
    }
    if (functionType.valueSet()) {
      doEncode(NextFree, CrntPos, ReadOnlyLValue::Create(IeleRValue::Create(ArgValue->getValues()[index])), ArgTypeSize, ArgLen, UInt, appendWidths, bigEndian);
      index++;
    }
    if (functionType.bound()) {
      std::vector<iele::IeleValue *> boundValue;
      boundValue.insert(boundValue.end(), ArgValue->getValues().begin() + index, ArgValue->getValues().end());
      doEncode(NextFree, CrntPos, ReadOnlyLValue::Create(IeleRValue::Create(boundValue)), ArgTypeSize, ArgLen, functionType.selfType(), appendWidths, bigEndian);
    }
    break;
  }
  default:
      solAssert(false, "IeleCompiler: invalid type for encoding");
  }
}

iele::IeleFunction *IeleCompiler::getRecursiveStructEncoder(
    const StructType &type, bool appendWidths, bool bigEndian) {
  solAssert(type.recursive(),
            "IeleCompiler: attempted to construct recursive destructor "
            "for non-recursive struct type");

  std::string typeID = type.richIdentifier();

  // Lookup encoder in the cache.
  auto It = RecursiveStructEncoders.find(typeID);
  if (It != RecursiveStructEncoders.end()) {
    auto ItAppendWidths = It->second.find(appendWidths);
    if (ItAppendWidths != It->second.end()) {
      auto ItBigEndian = ItAppendWidths->second.find(bigEndian);
      if (ItBigEndian != ItAppendWidths->second.end())
        return ItBigEndian->second;
    }
  }

  // Generate a recursive function to encode a struct of the specific type.
  iele::IeleFunction *Encoder =
    iele::IeleFunction::Create(
        &Context, false,
        "ielert.encode." + typeID + ".W" + (char)appendWidths +
        ".B" + (char)bigEndian,
        CompilingContract);

  // Register encoder function.
  RecursiveStructEncoders[typeID][appendWidths][bigEndian] = Encoder;

  // Add the address argument.
  iele::IeleArgument *Address =
    iele::IeleArgument::Create(&Context, "encode.addr", Encoder);

  // Add the next free argument.
  iele::IeleArgument *NextFree =
    iele::IeleArgument::Create(&Context, "encode.next.free", Encoder);

  // Add the current position argument.
  iele::IeleArgument *CrntPos =
    iele::IeleArgument::Create(&Context, "encode.crnt.pos", Encoder);

  // Add the address type size local variable.
  iele::IeleLocalVariable *AddrTypeSize =
    iele::IeleLocalVariable::Create(&Context, "encode.addr.type.size",
                                    CompilingFunction);

  // Add the address length local variable.
  iele::IeleLocalVariable *AddrLen =
    iele::IeleLocalVariable::Create(&Context, "encode.addr.len",
                                    CompilingFunction);

  // Add the first block.
  iele::IeleBlock *EncoderBlock =
    iele::IeleBlock::Create(&Context, "entry", Encoder);

  // Append the code for encoding the struct members and returning the current
  // position.
  std::swap(CompilingFunction, Encoder);
  std::swap(CompilingBlock, EncoderBlock);
  appendStructEncode(type, Address, AddrTypeSize, AddrLen, NextFree, CrntPos,
                     appendWidths, bigEndian);
  llvm::SmallVector<iele::IeleValue *, 1> Results(1, CrntPos);
  iele::IeleInstruction::CreateRet(Results, CompilingBlock);
  std::swap(CompilingFunction, Encoder);
  std::swap(CompilingBlock, EncoderBlock);

  return Encoder;
}

void IeleCompiler::appendStructEncode(
    const StructType &type, iele::IeleValue *Address,
    iele::IeleLocalVariable *AddrTypeSize, iele::IeleLocalVariable *AddrLen,
    iele::IeleValue *NextFree, iele::IeleLocalVariable *CrntPos,
    bool appendWidths, bool bigEndian) {
  // For each member of the struct, generate code that deletes that member.
  for (auto decl : type.structDefinition().members()) {
    // First compute the offset from the start of the struct.
    bigint offset;
    switch (type.location()) {
    case DataLocation::Storage: {
      const auto& offsets = type.storageOffsetsOfMember(decl->name());
      offset = offsets.first;
      break;
    }
    case DataLocation::CallData:
    case DataLocation::Memory: {
      offset = type.memoryOffsetOfMember(decl->name());
      break;
    }
    }

    iele::IeleLocalVariable *Member =
      iele::IeleLocalVariable::Create(&Context, "encode.address",
                                      CompilingFunction);
    iele::IeleValue *OffsetValue =
      iele::IeleIntConstant::Create(&Context, offset);
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Add, Member, Address, OffsetValue, CompilingBlock);

    TypePointer memberType =
      TypeProvider::withLocationIfReference(type.location(), decl->type());
    doEncode(NextFree, CrntPos, makeLValue(Member, memberType, type.location()),
             AddrTypeSize, AddrLen, memberType, appendWidths, bigEndian);
  }
}

void IeleCompiler::decoding(IeleRValue *encoded, TypePointers types,
                            llvm::SmallVectorImpl<IeleRValue *> &results) {
  iele::IeleLocalVariable *NextFree = appendMemorySpill();
  iele::IeleInstruction::CreateStore(
    encoded->getValue(), NextFree, CompilingBlock);

  iele::IeleLocalVariable *CrntPos =
    iele::IeleLocalVariable::Create(&Context, "crnt.pos", CompilingFunction);
  iele::IeleLocalVariable *ArgTypeSize =
    iele::IeleLocalVariable::Create(&Context, "arg.type.size",
                                    CompilingFunction);
  iele::IeleLocalVariable *ArgLen =
    iele::IeleLocalVariable::Create(&Context, "arg.len", CompilingFunction);
  iele::IeleInstruction::CreateAssign(
    CrntPos, iele::IeleIntConstant::getZero(&Context), CompilingBlock);

  for (TypePointer type : types) {
    iele::IeleLocalVariable *Result =
      iele::IeleLocalVariable::Create(&Context, "arg.value", CompilingFunction);
    doDecode(NextFree, CrntPos, RegisterLValue::Create({Result}), ArgTypeSize, ArgLen, type);
    results.push_back(IeleRValue::Create(Result));
  }
}

IeleRValue *IeleCompiler::decoding(IeleRValue *encoded, TypePointer type) {
  if (type->isValueType() || type->category() == Type::Category::Mapping) {
    return encoded;
  }

  // Allocate cell 
  iele::IeleLocalVariable *NextFree = appendMemorySpill();

  iele::IeleInstruction::CreateStore(
    encoded->getValue(), NextFree, CompilingBlock);

  iele::IeleLocalVariable *CrntPos = 
    iele::IeleLocalVariable::Create(&Context, "crnt.pos", CompilingFunction);
  iele::IeleLocalVariable *ArgTypeSize = 
    iele::IeleLocalVariable::Create(&Context, "arg.type.size", 
                                    CompilingFunction);
  iele::IeleLocalVariable *ArgLen = 
    iele::IeleLocalVariable::Create(&Context, "arg.len", CompilingFunction);
  iele::IeleInstruction::CreateAssign(
    CrntPos, iele::IeleIntConstant::getZero(&Context), CompilingBlock);    
  iele::IeleLocalVariable *Result =
    iele::IeleLocalVariable::Create(&Context, "arg.value", CompilingFunction);
  doDecode(NextFree, CrntPos, RegisterLValue::Create({Result}), ArgTypeSize, ArgLen, type);
  return IeleRValue::Create(Result);
}

void IeleCompiler::doDecode(
    iele::IeleValue *NextFree, iele::IeleLocalVariable *CrntPos,
    IeleLValue *StoreAt,
    iele::IeleLocalVariable *ArgTypeSize, iele::IeleLocalVariable *ArgLen,
    TypePointer type) {
  iele::IeleValue *AllocedValue;
  switch(type->category()) {
  case Type::Category::Contract:
  case Type::Category::FixedBytes:
  case Type::Category::Enum:
  case Type::Category::Bool:
  case Type::Category::Address:
  case Type::Category::Integer: {
    AllocedValue = iele::IeleLocalVariable::Create(&Context, "loaded.value", CompilingFunction);
    // width of given type, in bits, or 0 arbitrary precision
    bigint bitwidth = type->getFixedBitwidth();
    if (bitwidth != 0) { // Fixed-precision 
      // width of given type, in bytes
      bigint size = (bitwidth + 7) / 8;
      // codegen
      iele::IeleInstruction::CreateAssign(
        ArgTypeSize, 
        iele::IeleIntConstant::Create(&Context, size), 
        CompilingBlock);
      iele::IeleInstruction::CreateLoad(
        llvm::dyn_cast<iele::IeleLocalVariable>(AllocedValue), NextFree, CrntPos, 
        ArgTypeSize, CompilingBlock);
      iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Add, CrntPos, CrntPos, 
        ArgTypeSize, CompilingBlock);
      if (type->category() == Type::Category::Integer) {
        IntegerType const& intType = dynamic_cast<const IntegerType &>(*type);
        if (intType.isSigned()) {
          iele::IeleInstruction::CreateBinOp(
            iele::IeleInstruction::SExt,
            llvm::dyn_cast<iele::IeleLocalVariable>(AllocedValue), ArgTypeSize, AllocedValue,
            CompilingBlock);
	}
      }
    }
    else { // Arbitrary precision
      bigint argLenWidth = bigint(8); // Store length using 8 bytes 
      iele::IeleInstruction::CreateAssign(
        ArgTypeSize, iele::IeleIntConstant::Create(&Context, argLenWidth), 
        CompilingBlock);    
      iele::IeleInstruction::CreateLoad(
        ArgLen, NextFree, CrntPos, ArgTypeSize, CompilingBlock);
      iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Add, CrntPos, CrntPos, 
        ArgTypeSize, CompilingBlock);
      iele::IeleInstruction::CreateLoad(
        llvm::dyn_cast<iele::IeleLocalVariable>(AllocedValue), NextFree, CrntPos, ArgLen, CompilingBlock);
      iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::SExt,
        llvm::dyn_cast<iele::IeleLocalVariable>(AllocedValue), ArgLen, AllocedValue,
        CompilingBlock);
      iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Add, CrntPos, CrntPos, ArgLen, CompilingBlock);
    }
    IeleRValue *rvalue = IeleRValue::Create(AllocedValue);
    appendRangeCheck(rvalue, *type);
    StoreAt->write(rvalue, CompilingBlock);
    break;
  }
  case Type::Category::Mapping:
    // we do not actually encode mappings
    break;
  case Type::Category::Array: {
    const ArrayType &arrayType = dynamic_cast<const ArrayType &>(*type);
    TypePointer elementType = arrayType.baseType();
    if (arrayType.isByteArray()) {
      if (dynamic_cast<RegisterLValue *>(StoreAt)) {
        AllocedValue = appendArrayAllocation(arrayType);
        StoreAt->write(IeleRValue::Create(AllocedValue), CompilingBlock);
      } else {
        AllocedValue = StoreAt->read(CompilingBlock)->getValue();
      }

      iele::IeleLocalVariable *StringValue = iele::IeleLocalVariable::Create(&Context, "loaded.value", CompilingFunction);
      bigint argLenWidth = bigint(8); // Store length using 8 bytes 
      iele::IeleInstruction::CreateAssign(
        ArgTypeSize, iele::IeleIntConstant::Create(&Context, argLenWidth), 
        CompilingBlock);    
      iele::IeleInstruction::CreateLoad(
        ArgLen, NextFree, CrntPos, ArgTypeSize, CompilingBlock);
      iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Add, CrntPos, CrntPos, 
        ArgTypeSize, CompilingBlock);
      iele::IeleInstruction::CreateLoad(
        StringValue, NextFree, CrntPos, ArgLen, CompilingBlock);
      iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Twos,
        StringValue, ArgLen, StringValue,
        CompilingBlock);
      iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::BSwap, 
        StringValue, ArgLen, StringValue,
        CompilingBlock);
      iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Add, CrntPos, CrntPos, ArgLen, CompilingBlock);
      AddressLValue::Create(this, AllocedValue, DataLocation::Memory)->write(IeleRValue::Create(ArgLen), CompilingBlock);

      iele::IeleLocalVariable *Offset =
        iele::IeleLocalVariable::Create(&Context, "value.address", CompilingFunction);
      iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Add, Offset, AllocedValue, iele::IeleIntConstant::getOne(&Context),
        CompilingBlock);
      AddressLValue::Create(this, Offset, DataLocation::Memory)->write(IeleRValue::Create(StringValue), CompilingBlock);
    } else {
      iele::IeleLocalVariable *ArrayLen =
        iele::IeleLocalVariable::Create(&Context, "array.length", CompilingFunction);
      iele::IeleLocalVariable *Element =
        iele::IeleLocalVariable::Create(&Context, "array.element", CompilingFunction);
      if (arrayType.isDynamicallySized()) {
        iele::IeleInstruction::CreateAssign(
          ArgTypeSize, iele::IeleIntConstant::getOne(&Context),
          CompilingBlock);
        iele::IeleInstruction::CreateLoad(
          ArgLen, NextFree, CrntPos, ArgTypeSize, CompilingBlock);
        iele::IeleInstruction::CreateBinOp(
          iele::IeleInstruction::Add, CrntPos, CrntPos, ArgTypeSize,
          CompilingBlock);
        iele::IeleInstruction::CreateLoad(
          ArrayLen, NextFree, CrntPos, ArgLen, CompilingBlock);
        iele::IeleInstruction::CreateBinOp(
          iele::IeleInstruction::Add, CrntPos, CrntPos, ArgLen, CompilingBlock);
  
        AllocedValue = appendArrayAllocation(arrayType, ArrayLen);
        StoreAt->write(IeleRValue::Create(AllocedValue), CompilingBlock);
  
        iele::IeleInstruction::CreateBinOp(
          iele::IeleInstruction::Add,
          Element, AllocedValue, 
          iele::IeleIntConstant::getOne(&Context),
          CompilingBlock);
      } else {
        if (dynamic_cast<RegisterLValue *>(StoreAt)) {
          AllocedValue = appendArrayAllocation(arrayType);
          StoreAt->write(IeleRValue::Create(AllocedValue), CompilingBlock);
        } else {
          AllocedValue = StoreAt->read(CompilingBlock)->getValue();
        }
        iele::IeleInstruction::CreateAssign(
          ArrayLen,
          iele::IeleIntConstant::Create(&Context, arrayType.length()),
          CompilingBlock);
        iele::IeleInstruction::CreateAssign(
          Element, AllocedValue, CompilingBlock);
      }

      iele::IeleBlock *LoopBodyBlock =
        iele::IeleBlock::Create(&Context, "decode.loop", CompilingFunction);
      CompilingBlock = LoopBodyBlock;
  
      iele::IeleBlock *LoopExitBlock =
        iele::IeleBlock::Create(&Context, "decode.loop.end");
  
      iele::IeleLocalVariable *DoneValue =
        iele::IeleLocalVariable::Create(&Context, "done", CompilingFunction);
      iele::IeleInstruction::CreateIsZero(
        DoneValue, ArrayLen, CompilingBlock);
      connectWithConditionalJump(DoneValue, CompilingBlock, LoopExitBlock);
  
      bigint elementSize;
      switch (arrayType.location()) {
      case DataLocation::Storage: {
        elementSize = elementType->storageSize();
        break;
      }
      case DataLocation::CallData:
      case DataLocation::Memory: {
        elementSize = elementType->memorySize();
        break;
      }
      }
  
      doDecode(NextFree, CrntPos, makeLValue(Element, elementType, DataLocation::Memory), ArgTypeSize, ArgLen, elementType);
  
      iele::IeleValue *ElementSizeValue =
          iele::IeleIntConstant::Create(&Context, elementSize);
  
      iele::IeleInstruction::CreateBinOp(
          iele::IeleInstruction::Add, Element, Element,
          ElementSizeValue,
          CompilingBlock);
      iele::IeleInstruction::CreateBinOp(
          iele::IeleInstruction::Sub, ArrayLen, ArrayLen,
          iele::IeleIntConstant::getOne(&Context),
          CompilingBlock);
  
      connectWithUnconditionalJump(CompilingBlock, LoopBodyBlock);
      LoopExitBlock->insertInto(CompilingFunction);
      CompilingBlock = LoopExitBlock;
    }
    break;
  }
  case Type::Category::Struct: {
    const StructType &structType = dynamic_cast<const StructType &>(*type);
    if (dynamic_cast<RegisterLValue *>(StoreAt)) {
      AllocedValue = appendStructAllocation(structType);
      StoreAt->write(IeleRValue::Create(AllocedValue), CompilingBlock);
    } else {
      AllocedValue = StoreAt->read(CompilingBlock)->getValue();
    }

    if (structType.recursive()) {
      // Call the recursive decoder.
      iele::IeleFunction *Decoder = getRecursiveStructDecoder(structType);
      llvm::SmallVector<iele::IeleLocalVariable *, 1> Results(1, CrntPos);
      llvm::SmallVector<iele::IeleValue *, 3> Arguments;
      Arguments.push_back(AllocedValue);
      Arguments.push_back(NextFree);
      Arguments.push_back(CrntPos);
      iele::IeleInstruction::CreateInternalCall(
          Results, Decoder, Arguments, CompilingBlock);
      break;
    }

    appendStructDecode(structType, AllocedValue, ArgTypeSize, ArgLen, NextFree,
                       CrntPos);
    break;
  }
  case Type::Category::Function: {
    const FunctionType &functionType = dynamic_cast<const FunctionType &>(*type);
    FunctionType::Kind kind;
    if (functionType.kind() == FunctionType::Kind::SetGas || functionType.kind() == FunctionType::Kind::SetValue)
    {
      solAssert(functionType.returnParameterTypes().size() == 1, "");
      kind = dynamic_cast<FunctionType const&>(*functionType.returnParameterTypes().front()).kind();
    } else {
      kind = functionType.kind();
    }
    std::vector<iele::IeleLocalVariable *> Results;
    std::vector<iele::IeleValue *> ResultValues;
    for (unsigned i = 0; i < functionType.sizeInRegisters(); i++) {
      auto reg = iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
      Results.push_back(reg);
      ResultValues.push_back(reg);
    }
    unsigned index = 0;
    switch (kind) {
    case FunctionType::Kind::External:
      doDecode(NextFree, CrntPos, RegisterLValue::Create({Results[0]}), ArgTypeSize, ArgLen, Address);
      doDecode(NextFree, CrntPos, RegisterLValue::Create({Results[1]}), ArgTypeSize, ArgLen, UInt16);
      index = 2;
      break;
    case FunctionType::Kind::BareCall:
    case FunctionType::Kind::BareCallCode:
    case FunctionType::Kind::BareDelegateCall:
    case FunctionType::Kind::BareStaticCall:
    case FunctionType::Kind::Internal:
    case FunctionType::Kind::DelegateCall:
      doDecode(NextFree, CrntPos, RegisterLValue::Create({Results[0]}), ArgTypeSize, ArgLen, UInt16);
      index = 1;
      break;
    case FunctionType::Kind::ArrayPush:
    case FunctionType::Kind::ByteArrayPush:
    case FunctionType::Kind::ArrayPop:
      solAssert(false, "not implemented yet");
    default:
      break;
    }
    if (functionType.gasSet()) {
      doDecode(NextFree, CrntPos, RegisterLValue::Create({Results[index]}), ArgTypeSize, ArgLen, UInt);
      index++;
    }
    if (functionType.valueSet()) {
      doDecode(NextFree, CrntPos, RegisterLValue::Create({Results[index]}), ArgTypeSize, ArgLen, UInt);
      index++;
    }
    if (functionType.bound()) {
      std::vector<iele::IeleLocalVariable *> boundValue;
      boundValue.insert(boundValue.end(), Results.begin() + index, Results.end());
      doDecode(NextFree, CrntPos, RegisterLValue::Create(boundValue), ArgTypeSize, ArgLen, functionType.selfType());
    }
    StoreAt->write(IeleRValue::Create(ResultValues), CompilingBlock);
    break;
  }

  default: 
    solAssert(false, "invalid reference type");
    break;
  }
}

iele::IeleFunction *IeleCompiler::getRecursiveStructDecoder(
    const StructType &type) {
  solAssert(type.recursive(),
            "IeleCompiler: attempted to construct recursive destructor "
            "for non-recursive struct type");

  std::string typeID = type.richIdentifier();

  // Lookup decoder in the cache.
  auto It = RecursiveStructDecoders.find(typeID);
  if (It != RecursiveStructDecoders.end())
    return It->second;

  // Generate a recursive function to decode a struct of the specific type.
  iele::IeleFunction *Decoder =
    iele::IeleFunction::Create(
        &Context, false, "ielert.decode." + typeID, CompilingContract);

  // Register decoder function.
  RecursiveStructDecoders[typeID] = Decoder;

  // Add the address argument.
  iele::IeleArgument *Address =
    iele::IeleArgument::Create(&Context, "decode.addr", Decoder);

  // Add the next free argument.
  iele::IeleArgument *NextFree =
    iele::IeleArgument::Create(&Context, "decode.next.free", Decoder);

  // Add the current position argument.
  iele::IeleArgument *CrntPos =
    iele::IeleArgument::Create(&Context, "decode.crnt.pos", Decoder);

  // Add the address type size local variable.
  iele::IeleLocalVariable *AddrTypeSize =
    iele::IeleLocalVariable::Create(&Context, "decode.addr.type.size",
                                    CompilingFunction);

  // Add the address length local variable.
  iele::IeleLocalVariable *AddrLen =
    iele::IeleLocalVariable::Create(&Context, "decode.addr.len",
                                    CompilingFunction);

  // Add the first block.
  iele::IeleBlock *DecoderBlock =
    iele::IeleBlock::Create(&Context, "entry", Decoder);

  // Append the code for decoding the struct members and returning the current
  // position.
  std::swap(CompilingFunction, Decoder);
  std::swap(CompilingBlock, DecoderBlock);
  appendStructDecode(type, Address, AddrTypeSize, AddrLen, NextFree, CrntPos);
  llvm::SmallVector<iele::IeleValue *, 1> Results(1, CrntPos);
  iele::IeleInstruction::CreateRet(Results, CompilingBlock);
  std::swap(CompilingFunction, Decoder);
  std::swap(CompilingBlock, DecoderBlock);

  return Decoder;
}

void IeleCompiler::appendStructDecode(
    const StructType &type, iele::IeleValue *Address,
    iele::IeleLocalVariable *AddrTypeSize, iele::IeleLocalVariable *AddrLen,
    iele::IeleValue *NextFree, iele::IeleLocalVariable *CrntPos) {
  for (auto decl : type.structDefinition().members()) {
    // First compute the offset from the start of the struct.
    bigint offset;
    switch (type.location()) {
    case DataLocation::Storage: {
      const auto& offsets = type.storageOffsetsOfMember(decl->name());
      offset = offsets.first;
      break;
    }
    case DataLocation::CallData:
    case DataLocation::Memory: {
      offset = type.memoryOffsetOfMember(decl->name());
      break;
    }
    }

    iele::IeleLocalVariable *Member =
      iele::IeleLocalVariable::Create(&Context, "encode.address",
                                      CompilingFunction);
    iele::IeleValue *OffsetValue =
      iele::IeleIntConstant::Create(&Context, offset);
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Add, Member, Address, OffsetValue,
      CompilingBlock);

    doDecode(
        NextFree, CrntPos,
        makeLValue(Member, decl->type(), DataLocation::Memory), AddrTypeSize,
        AddrLen, decl->type());
  }
}

static bigint log2(bigint op) {
  bigint result = 0;
  while (op > 1) {
    op >>= 1;
    result += 1;
  }
  return result; 
}

void IeleCompiler::appendMul(iele::IeleLocalVariable *LValue, iele::IeleValue *LeftOperand, bigint RightOperand) {
  if (RightOperand == 0) {
    iele::IeleInstruction::CreateAssign(
      LValue, iele::IeleIntConstant::getZero(&Context), CompilingBlock);
  } else if (RightOperand == 1) {
    iele::IeleInstruction::CreateAssign(
      LValue, LeftOperand, CompilingBlock);
  } else if ((RightOperand & (RightOperand - 1)) == 0) {
    // is a power of two
    bigint bits = log2(RightOperand);
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Shift,
      LValue, LeftOperand, iele::IeleIntConstant::Create(&Context, bits), CompilingBlock);
  } else {
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Mul,
      LValue, LeftOperand, iele::IeleIntConstant::Create(&Context, RightOperand), CompilingBlock);
  }
}

bool IeleCompiler::visit(const FunctionCall &functionCall) {
  // Not supported yet cases.
  if (*functionCall.annotation().kind == FunctionCallKind::TypeConversion) {
    solAssert(functionCall.arguments().size() == 1, "");
    solAssert(functionCall.names().empty(), "");
    IeleRValue *ArgumentValue =
      compileExpression(*functionCall.arguments().front().get());
    solAssert(ArgumentValue,
           "IeleCompiler: Failed to compile conversion target.");
    IeleRValue *ResultValue = appendTypeConversion(ArgumentValue,
      functionCall.arguments().front()->annotation().type,
      functionCall.annotation().type);
    CompilingExpressionResult.push_back(ResultValue);
    return false;
  }

  std::vector<ASTPointer<const Expression>> arguments;
  FunctionTypePointer functionType;
  auto callArgumentNames = functionCall.names();  
  auto callArguments = functionCall.arguments();

  if (*functionCall.annotation().kind ==
        FunctionCallKind::StructConstructorCall) {
    const TypeType &type =
      dynamic_cast<const TypeType &>(
          *functionCall.expression().annotation().type);
    const StructType &structType =
      dynamic_cast<const StructType &>(*type.actualType());
    functionType = structType.constructorType();

    solAssert(functionType->parameterTypes().size() ==
        structType.memoryMemberTypes().size(),
        "IeleCompiler: struct constructor with missing arguments");
  } else {
    functionType = dynamic_cast<FunctionTypePointer>(
        functionCall.expression().annotation().type);
  }

  if (!callArgumentNames.empty()) {
    for (auto const& parameterName: functionType->parameterNames()) {
      bool found = false;
      for (size_t j = 0; j < callArgumentNames.size() && !found; j++)
        if ((found = (parameterName == *callArgumentNames[j])))
          arguments.push_back(callArguments[j]);
      solAssert(found, "");
    }
  }
  else {
    arguments = callArguments;
  }

  if (*functionCall.annotation().kind ==
        FunctionCallKind::StructConstructorCall) {
    const TypeType &type =
      dynamic_cast<const TypeType &>(
          *functionCall.expression().annotation().type);
    const StructType &structType =
      dynamic_cast<const StructType &>(*type.actualType());

    solAssert(arguments.size() == functionType->parameterTypes().size(),
              "IeleCompiler: struct constructor called with wrong number "
              "of arguments");

    // Allocate memory for the struct.
    iele::IeleValue *StructValue = appendStructAllocation(structType);

    // Visit arguments and initialize struct fields.
    bigint offset = 0; 
    for (unsigned i = 0; i < arguments.size(); ++i) {
      // Visit argument.
      IeleRValue *InitValue = compileExpression(*arguments[i]);
      TypePointer argType = arguments[i]->annotation().type;
      TypePointer memberType = functionType->parameterTypes()[i];
      InitValue = appendTypeConversion(InitValue, argType, memberType);

      // Compute lvalue in the new struct in which we should assign the argument
      // value.
      iele::IeleLocalVariable *AddressValue =
        iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
      iele::IeleInstruction::CreateBinOp(
          iele::IeleInstruction::Add, AddressValue, StructValue,
          iele::IeleIntConstant::Create(&Context, offset), CompilingBlock);
      IeleLValue *MemberLValue =
        makeLValue(AddressValue, memberType, DataLocation::Memory);

      // Do the assignment.
      argType = argType->mobileType();
      solAssert(!shouldCopyStorageToStorage(*memberType, MemberLValue, *argType),
                "IeleCompiler: storage to storage copy needed in "
                "initialization of dynamically allocated struct.");
      solAssert(!shouldCopyMemoryToStorage(*memberType, MemberLValue, *argType),
                "IeleCompiler: memory to storage copy needed in "
                "initialization of dynamically allocated struct.");
      if (shouldCopyMemoryToMemory(*memberType, MemberLValue, *argType)) {
        appendCopyFromMemoryToMemory(
            MemberLValue, memberType, InitValue, argType);
      } else {
        IeleLValue *lvalue =
          AddressLValue::Create(this, AddressValue, DataLocation::Memory,
                                "loaded", memberType->sizeInRegisters());
        lvalue->write(InitValue, CompilingBlock);
      }

      offset += memberType->memorySize();
    }

    // Return pointer to allocated struct.
    CompilingExpressionResult.push_back(IeleRValue::Create(StructValue));

    return false;
  }


  const FunctionType &function = *functionType;

  switch (function.kind()) {
  case FunctionType::Kind::Send:
  case FunctionType::Kind::Transfer: {
    // Get target address.
    iele::IeleValue *TargetAddressValue =
      compileExpression(functionCall.expression())->getValue();
    solAssert(TargetAddressValue,
           "IeleCompiler: Failed to compile transfer target address.");

    // Get transfer value.
    IeleRValue *TransferValue =
      compileExpression(*arguments.front().get());
    TransferValue = appendTypeConversion(TransferValue,
      arguments.front()->annotation().type,
      function.parameterTypes().front());
    solAssert(TransferValue,
           "IeleCompiler: Failed to compile transfer value.");

    llvm::SmallVector<iele::IeleValue *, 0> EmptyArguments;
    llvm::SmallVector<iele::IeleLocalVariable *, 0> EmptyLValues;

    llvm::SmallVector<IeleLValue *, 4> Results;
    appendConditional(TransferValue->getValue(), Results,
      [this](llvm::SmallVectorImpl<IeleRValue *> &Results){
        Results.push_back(IeleRValue::Create(iele::IeleIntConstant::getZero(&Context)));
      },
      [this](llvm::SmallVectorImpl<IeleRValue *> &Results){
        Results.push_back(IeleRValue::Create(iele::IeleIntConstant::Create(&Context, 2300)));
      });
    iele::IeleValue *GasValue = Results[0]->read(CompilingBlock)->getValue();

    // Create call to deposit.
    iele::IeleGlobalVariable *Deposit =
      iele::IeleGlobalVariable::Create(&Context, "deposit");
    iele::IeleLocalVariable *StatusValue =
      CompilingFunctionStatus;
    iele::IeleInstruction::CreateAccountCall(false, StatusValue, EmptyLValues, Deposit,
                                             TargetAddressValue,
                                             TransferValue->getValue(), GasValue,
                                             EmptyArguments,
                                             CompilingBlock);
    
    if (function.kind() == FunctionType::Kind::Transfer) {
      // For transfer revert if status is not zero.
      appendRevert(StatusValue, StatusValue);
    } else {
      iele::IeleLocalVariable *ResultValue =
        iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
      iele::IeleInstruction::CreateIsZero(ResultValue, StatusValue, CompilingBlock);
      CompilingExpressionResult.push_back(IeleRValue::Create(ResultValue));
    }
    break;
  }
  case FunctionType::Kind::Revert: {
      iele::IeleValue *MessageValue = nullptr;
      if (arguments.size() > 0) {
        IeleRValue *Message = compileExpression(*arguments[0]);
        TypePointer ArgType = arguments[0]->annotation().type;
        auto ParamType = function.parameterTypes()[0];
        Message = appendTypeConversion(Message, ArgType, ParamType);

        iele::IeleLocalVariable *NextFree = appendMemorySpill();
        llvm::SmallVector<IeleRValue *, 1> args;
        args.push_back(Message);
        TypePointers types;
        types.push_back(ParamType);
        encoding(args, types, NextFree, false);
        iele::IeleInstruction::CreateLoad(CompilingFunctionStatus, NextFree, CompilingBlock);
        MessageValue = CompilingFunctionStatus;
      }
      appendRevert(nullptr, MessageValue);
      break;
  }
  case FunctionType::Kind::Assert:
  case FunctionType::Kind::Require: {
    // Visit condition.
    IeleRValue *ConditionValue =
      compileExpression(*arguments.front().get());
    ConditionValue = appendTypeConversion(ConditionValue,
      arguments.front()->annotation().type,
      function.parameterTypes().front());
    solAssert(ConditionValue,
           "IeleCompiler: Failed to compile require/assert condition.");

    // Create check for false.
    iele::IeleLocalVariable *InvConditionValue =
      iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
    iele::IeleInstruction::CreateIsZero(InvConditionValue, ConditionValue->getValue(),
                                        CompilingBlock);
    if (function.kind() == FunctionType::Kind::Assert)
      appendRevert(InvConditionValue, nullptr);
    else {
      iele::IeleValue *MessageValue = nullptr;
      if (arguments.size() > 1) {
        IeleRValue *Message = compileExpression(*arguments[1]);
        TypePointer ArgType = arguments[1]->annotation().type;
        auto ParamType = function.parameterTypes()[1];
        Message = appendTypeConversion(Message, ArgType, ParamType);

        iele::IeleLocalVariable *NextFree = appendMemorySpill();
        llvm::SmallVector<IeleRValue *, 1> args;
        args.push_back(Message);
        TypePointers types;
        types.push_back(ParamType);
        encoding(args, types, NextFree, false);
        iele::IeleInstruction::CreateLoad(CompilingFunctionStatus, NextFree, CompilingBlock);
        MessageValue = CompilingFunctionStatus;
      }
      appendRevert(InvConditionValue, MessageValue);
    }
    break;
  }
  case FunctionType::Kind::AddMod: {
    IeleRValue *Op1 = compileExpression(*arguments[0].get());
    Op1 = appendTypeConversion(Op1, arguments[0]->annotation().type, UInt);
    solAssert(Op1,
           "IeleCompiler: Failed to compile operand 1 of addmod.");
    IeleRValue *Op2 = compileExpression(*arguments[1].get());
    Op2 = appendTypeConversion(Op2, arguments[1]->annotation().type, UInt);
    solAssert(Op2,
           "IeleCompiler: Failed to compile operand 2 of addmod.");
    IeleRValue *Modulus = compileExpression(*arguments[2].get());
    Modulus = appendTypeConversion(Modulus, arguments[2]->annotation().type, UInt);
    solAssert(Modulus,
           "IeleCompiler: Failed to compile modulus of addmod.");

    iele::IeleLocalVariable *AddModValue =
      iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
    iele::IeleInstruction::CreateTernOp(iele::IeleInstruction::AddMod,
                                        AddModValue, Op1->getValue(), Op2->getValue(), Modulus->getValue(), 
                                        CompilingBlock);
    CompilingExpressionResult.push_back(IeleRValue::Create(AddModValue));
    break;
  }
  case FunctionType::Kind::MulMod: {
    IeleRValue *Op1 = compileExpression(*arguments[0].get());
    Op1 = appendTypeConversion(Op1, arguments[0]->annotation().type, UInt);
    solAssert(Op1,
           "IeleCompiler: Failed to compile operand 1 of addmod.");
    IeleRValue *Op2 = compileExpression(*arguments[1].get());
    Op2 = appendTypeConversion(Op2, arguments[1]->annotation().type, UInt);
    solAssert(Op2,
           "IeleCompiler: Failed to compile operand 2 of addmod.");
    IeleRValue *Modulus = compileExpression(*arguments[2].get());
    Modulus = appendTypeConversion(Modulus, arguments[2]->annotation().type, UInt);
    solAssert(Modulus,
           "IeleCompiler: Failed to compile modulus of addmod.");

    iele::IeleLocalVariable *MulModValue =
      iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
    iele::IeleInstruction::CreateTernOp(iele::IeleInstruction::MulMod,
                                        MulModValue, Op1->getValue(), Op2->getValue(), Modulus->getValue(), 
                                        CompilingBlock);
    CompilingExpressionResult.push_back(IeleRValue::Create(MulModValue));
    break;
  }
  case FunctionType::Kind::ObjectCreation: {
    solAssert(arguments.size() == 1,
              "IeleCompiler: invalid arguments for array creation");

    // Visit argument to get the requested size.
    iele::IeleValue *ArraySizeValue = compileExpression(*arguments[0])->getValue();

    // Allocate memory for the array.
    const ArrayType &arrayType =
      dynamic_cast<const ArrayType &>(*functionCall.annotation().type);
    solAssert(arrayType.isDynamicallySized(),
              "IeleCompiler: found fix-sized array type in new expression");
    solAssert(arrayType.dataStoredIn(DataLocation::Memory),
              "IeleCompiler: found storage allocation with new");
    iele::IeleValue *ArrayValue;
    if (arrayType.isByteArray()) {
      ArrayValue = appendArrayAllocation(arrayType);
      iele::IeleInstruction::CreateStore(
        ArraySizeValue, ArrayValue, CompilingBlock);
    } else {
      ArrayValue = appendArrayAllocation(arrayType, ArraySizeValue);
    }

    // Return pointer to allocated array.
    CompilingExpressionResult.push_back(IeleRValue::Create(ArrayValue));
    break;
  }
  case FunctionType::Kind::DelegateCall:
  case FunctionType::Kind::Internal: {
    bool isDelegateCall = function.kind() == FunctionType::Kind::DelegateCall;
    // Visit arguments.
    llvm::SmallVector<iele::IeleValue *, 4> Arguments;
    llvm::SmallVector<iele::IeleLocalVariable *, 4> ReturnRegisters;
    llvm::SmallVector<IeleLValue *, 4> Returns;
    compileFunctionArguments(Arguments, ReturnRegisters, Returns, arguments, function, false);

    // Visit callee.
    IeleRValue *CalleeValues = compileExpression(functionCall.expression());
    iele::IeleValue *CalleeValue = CalleeValues->getValues()[0];
    if (CalleeValues->getValues().size() > 1) {
      Arguments.insert(Arguments.begin(), CalleeValues->getValues().begin() + 1, CalleeValues->getValues().end());
    }
    if (hasTwoFunctions(function, false, isDelegateCall)) {
      CalleeValue = convertFunctionToInternal(CalleeValue);
    }
    solAssert(CalleeValue,
              "IeleCompiler: Failed to compile callee of internal function "
              "call");

    // Generate call and return values.
    iele::IeleInstruction::CreateInternalCall(ReturnRegisters, CalleeValue, Arguments,
                                              CompilingBlock);
    CompilingExpressionResult.insert(
        CompilingExpressionResult.end(), Returns.begin(), Returns.end());
    break;
  }
  case FunctionType::Kind::Selfdestruct: {
    // Visit argument (target of the Selfdestruct)
    IeleRValue *TargetAddress =
      compileExpression(*arguments.front().get());
    TargetAddress = appendTypeConversion(TargetAddress,
      arguments.front()->annotation().type,
      function.parameterTypes().front());
    solAssert(TargetAddress,
              "IeleCompiler: Failed to compile Selfdestruct target.");

    // Make IELE instruction
    iele::IeleInstruction::CreateSelfdestruct(TargetAddress->getValue(), CompilingBlock);
    break;
  }
  case FunctionType::Kind::Event: {
    llvm::SmallVector<iele::IeleValue *, 4> IndexedArguments;
    llvm::SmallVector<IeleRValue *, 4> NonIndexedArguments;
    TypePointers nonIndexedTypes;
    unsigned numIndexed = 0;
    
    // Get the event definition
    auto const& event = 
      dynamic_cast<EventDefinition const&>(function.declaration());
    
    // Include Event ID as first indexed argument
    // (but only if event is not anonymous!)
    if (!event.isAnonymous()) {
      iele::IeleIntConstant *EventID =
        iele::IeleIntConstant::Create(
          &Context,
          bigint(u256(h256::Arith(util::keccak256(
            function.externalSignature())))),
          true /* print as hex */);
      IndexedArguments.push_back(EventID);
      ++numIndexed;
    }

    // Visit indexed arguments.
    for (unsigned arg = 0; arg < arguments.size(); ++arg) {
      if (event.parameters()[arg]->isIndexed()) {
        // Compile argument
        IeleRValue *ArgValue = compileExpression(*arguments[arg]);
        solAssert(ArgValue,
                  "IeleCompiler: Failed to compile indexed event argument ");
        auto ArgType = arguments[arg]->annotation().type;
        auto ParamType = function.parameterTypes()[arg];
        numIndexed+=ParamType->sizeInRegisters();
        ArgValue = appendTypeConversion(ArgValue, ArgType, ParamType);
        ArgValue = encoding(ArgValue, ParamType, true);
        IndexedArguments.insert(IndexedArguments.end(), ArgValue->getValues().begin(), ArgValue->getValues().end());
      }
    }

    // Max 4 indexed params allowed! 
    solAssert(numIndexed <= 4, "Too many indexed arguments.");
    
    // Visit non indexed params
    for (unsigned arg = 0; arg < arguments.size(); ++arg) {
      if (!event.parameters()[arg]->isIndexed()) {
        // Compile argument
        IeleRValue *ArgValue = compileExpression(*arguments[arg]);
        solAssert(ArgValue,
                  "IeleCompiler: Failed to compile non-indexed event argument");
        // Store indexed argument
        auto ArgType = arguments[arg]->annotation().type;
        auto ParamType = function.parameterTypes()[arg];
        ArgValue = appendTypeConversion(ArgValue, ArgType, ParamType);
        NonIndexedArguments.push_back(ArgValue);
        nonIndexedTypes.push_back(ParamType);
      }
    }
    // Find out next free location (will store encoding of non-indexed args)     
    iele::IeleLocalVariable *NextFree = appendMemorySpill();

    // Store non-indexed args in memory
    if (NonIndexedArguments.size() > 0) {
      encoding(
        NonIndexedArguments, nonIndexedTypes, 
        NextFree, true);
    }

    // build Log instruction
    iele::IeleInstruction::CreateLog(
      // Contains encoded data, or is uninitilised in case of no non-indexed args
      IndexedArguments, NextFree, CompilingBlock);
    break;
  }
  case FunctionType::Kind::SetGas: {
    IeleRValue *CalleeValue = compileExpression(functionCall.expression());

    IeleRValue *GasValue = compileExpression(*arguments.front());
    GasValue = appendTypeConversion(GasValue, arguments.front()->annotation().type, UInt);
    std::vector<iele::IeleValue *> values;
    values.insert(values.end(), CalleeValue->getValues().begin(), CalleeValue->getValues().end());
    auto &baseType = dynamic_cast<const FunctionType &>(*function.returnParameterTypes().front());
    values.insert(values.begin() + baseType.gasIndex(), GasValue->getValue());
    CompilingExpressionResult.push_back(IeleRValue::Create(values));
    break;
  }
  case FunctionType::Kind::SetValue: {
    IeleRValue *CalleeValue = compileExpression(functionCall.expression());

    IeleRValue *TransferValue = compileExpression(*arguments.front());
    TransferValue = appendTypeConversion(TransferValue, arguments.front()->annotation().type, UInt);
    std::vector<iele::IeleValue *> values;
    values.insert(values.end(), CalleeValue->getValues().begin(), CalleeValue->getValues().end());
    auto &baseType = dynamic_cast<const FunctionType &>(*function.returnParameterTypes().front());
    values.insert(values.begin() + baseType.valueIndex(), TransferValue->getValue());
    CompilingExpressionResult.push_back(IeleRValue::Create(values));
    break;
  }
  case FunctionType::Kind::GasLeft: {
    appendGasLeft();
    break;
  }
  case FunctionType::Kind::External: {
    // Visit arguments.
    llvm::SmallVector<iele::IeleValue *, 4> Arguments;
    llvm::SmallVector<iele::IeleLocalVariable *, 4> ReturnRegisters;
    llvm::SmallVector<IeleLValue *, 4> Returns;
    compileFunctionArguments(Arguments, ReturnRegisters, Returns, arguments, function, true);

    IeleRValue *CalleeValue = compileExpression(functionCall.expression());
    iele::IeleValue *FunctionCalleeValue = CalleeValue->getValues()[1];
    iele::IeleValue *AddressValue =
      CalleeValue->getValues()[0];

    iele::IeleLocalVariable *StatusValue =
      CompilingFunctionStatus;

    bool StaticCall =
      function.stateMutability() <= StateMutability::View;

    solAssert(!StaticCall || !function.valueSet(), "Value set for staticcall");

    iele::IeleValue *TransferValue = nullptr;
    if (!StaticCall && !function.valueSet()) {
      TransferValue = iele::IeleIntConstant::getZero(&Context);
    } else if (!StaticCall) {
      TransferValue = CalleeValue->getValues()[function.gasSet() ? 3 : 2];
    }

    iele::IeleValue *GasValue;
    if (!function.gasSet()) {
      llvm::SmallVector<iele::IeleValue *, 0> EmptyArguments;
      iele::IeleLocalVariable *GasLeft =
        iele::IeleLocalVariable::Create(&Context, "gas", CompilingFunction);
      iele::IeleInstruction::CreateIntrinsicCall(
        iele::IeleInstruction::Gas, GasLeft, EmptyArguments,
        CompilingBlock);
      GasValue = GasLeft;
    } else {
      GasValue = CalleeValue->getValues()[2];
    }

    iele::IeleInstruction::CreateAccountCall(
      StaticCall, StatusValue, ReturnRegisters, FunctionCalleeValue,
      AddressValue, TransferValue, GasValue, Arguments,
      CompilingBlock);

    appendRevert(StatusValue, StatusValue);

    llvm::SmallVector<IeleRValue *, 4> DecodedReturns;
    for (unsigned i = 0; i < Returns.size(); i++) {
      IeleLValue *Return = Returns[i];
      TypePointer type = function.returnParameterTypes()[i];
      DecodedReturns.push_back(decoding(Return->read(CompilingBlock), type));
    }

    CompilingExpressionResult.insert(
        CompilingExpressionResult.end(), DecodedReturns.begin(), DecodedReturns.end());
    break;
  }
  case FunctionType::Kind::BareCall:
  case FunctionType::Kind::BareCallCode:
  case FunctionType::Kind::BareDelegateCall:
  case FunctionType::Kind::BareStaticCall:
    solAssert(false, "low-level function calls not supported in IELE");
  case FunctionType::Kind::Creation: {
    solAssert(!function.gasSet(), "Gas limit set for contract creation.");
    llvm::SmallVector<iele::IeleValue *, 4> Arguments;
    llvm::SmallVector<iele::IeleLocalVariable *, 4> ReturnRegisters;
    llvm::SmallVector<IeleLValue *, 4> Returns;
    compileFunctionArguments(Arguments, ReturnRegisters, Returns, arguments, function, true);
    solAssert(Returns.size() == 1, "New expression returns a single result.");

    IeleRValue *Callee = compileExpression(functionCall.expression());
    auto Contract = dynamic_cast<iele::IeleContract *>(Callee->getValues()[0]);
    solAssert(Contract, "invalid value passed to contract creation");

    iele::IeleLocalVariable *StatusValue =
      CompilingFunctionStatus;

    iele::IeleValue *TransferValue = nullptr;
    if (!function.valueSet()) {
      TransferValue = iele::IeleIntConstant::getZero(&Context);
    } else {
      TransferValue = Callee->getValues()[1];
    }

    iele::IeleInstruction::CreateCreate(
      false, StatusValue, ReturnRegisters[0], Contract, nullptr,
      TransferValue, Arguments, CompilingBlock);

    appendRevert(StatusValue, StatusValue);

    CompilingExpressionResult.insert(
        CompilingExpressionResult.end(), Returns.begin(), Returns.end());
    break;
  }
/*
  case FunctionType::Kind::Log0:
  case FunctionType::Kind::Log1:
  case FunctionType::Kind::Log2:
  case FunctionType::Kind::Log3:
  case FunctionType::Kind::Log4: {
    llvm::SmallVector<iele::IeleValue *, 4> CompiledArguments;

    // First arg is the data part
    iele::IeleValue *DataArg = compileExpression(*arguments[0])->getValue();
    solAssert(DataArg,
              "IeleCompiler: Failed to compile data argument of log operation ");

    // Remaining args are the topics
    for (unsigned arg = 1; arg < arguments.size(); arg++)
    {
        iele::IeleValue *ArgValue = compileExpression(*arguments[arg])->getValue();
        solAssert(ArgValue,
                  "IeleCompiler: Failed to compile indexed log argument ");
        CompiledArguments.push_back(ArgValue);
    }

    // Find out next free location (will store encoding of non-indexed args)     
    // TODO: this is the same as in the events code. Refactor after encodings are done?
    iele::IeleLocalVariable *NextFree = appendMemorySpill();

    // Store the data argument in memory
    iele::IeleInstruction::CreateStore(DataArg,
                                       NextFree, 
                                       iele::IeleIntConstant::getZero(&Context),
                                       iele::IeleIntConstant::Create(&Context, bigint(32)),
                                       CompilingBlock);

    // make log instruction 
    iele::IeleInstruction::CreateLog(CompiledArguments,
                                     NextFree,
                                     CompilingBlock);

    break;
  }
*/
  case FunctionType::Kind::ECAdd: {
    // Visit arguments.
    llvm::SmallVector<iele::IeleValue *, 4> Arguments;
    llvm::SmallVector<iele::IeleLocalVariable *, 2> ReturnRegisters;
    iele::IeleLocalVariable *ResultX =
      iele::IeleLocalVariable::Create(&Context, "ecadd.x", CompilingFunction);
    iele::IeleLocalVariable *ResultY =
      iele::IeleLocalVariable::Create(&Context, "ecadd.y", CompilingFunction);
    ReturnRegisters.push_back(ResultX);
    ReturnRegisters.push_back(ResultY);

    for (unsigned i = 0; i < arguments.size(); ++i) {
      IeleRValue *ArgValue = compileExpression(*arguments[i]);
      solAssert(ArgValue,
                "IeleCompiler: Failed to compile internal function call "
                "argument");
      // Check if we need to do a memory to/from storage copy.
      TypePointer ArgType = arguments[i]->annotation().type;
      TypePointer ParamType = function.parameterTypes()[i];
      auto arrayType = dynamic_cast<const ArrayType *>(ParamType);
      ArgValue = appendTypeConversion(ArgValue, ArgType, ParamType);
      iele::IeleValue *x = appendArrayAccess(*arrayType, iele::IeleIntConstant::getZero(&Context),
          ArgValue->getValue(), arrayType->location())->read(CompilingBlock)->getValue();
      iele::IeleValue *y = appendArrayAccess(*arrayType, iele::IeleIntConstant::getOne(&Context),
          ArgValue->getValue(), arrayType->location())->read(CompilingBlock)->getValue();
      Arguments.push_back(x);
      Arguments.push_back(y);
    }

    IeleRValue *CalleeValue = compileExpression(functionCall.expression());
    iele::IeleGlobalVariable *FunctionCalleeValue =
      iele::IeleGlobalVariable::Create(&Context, "iele.ecadd");
    iele::IeleValue *AddressValue =
      iele::IeleIntConstant::getOne(&Context);

    iele::IeleLocalVariable *StatusValue =
      CompilingFunctionStatus;

    iele::IeleValue *GasValue;
    if (!function.gasSet()) {
      llvm::SmallVector<iele::IeleValue *, 0> EmptyArguments;
      iele::IeleLocalVariable *GasLeft =
        iele::IeleLocalVariable::Create(&Context, "gas", CompilingFunction);
      iele::IeleInstruction::CreateIntrinsicCall(
        iele::IeleInstruction::Gas, GasLeft, EmptyArguments,
        CompilingBlock);
      GasValue = GasLeft;
    } else {
      GasValue = CalleeValue->getValues()[0];
    }

    iele::IeleInstruction::CreateAccountCall(
      true, StatusValue, ReturnRegisters, FunctionCalleeValue, AddressValue,
      nullptr, GasValue, Arguments, CompilingBlock);

    appendRevert(StatusValue, StatusValue);

    auto returnType = dynamic_cast<const ArrayType *>(function.returnParameterTypes()[0]);
    iele::IeleValue *Return = appendArrayAllocation(*returnType);
    appendArrayAccess(*returnType, iele::IeleIntConstant::getZero(&Context),
      Return, returnType->location())->write(IeleRValue::Create(ReturnRegisters[0]), CompilingBlock);
    appendArrayAccess(*returnType, iele::IeleIntConstant::getOne(&Context),
      Return, returnType->location())->write(IeleRValue::Create(ReturnRegisters[1]), CompilingBlock);
    CompilingExpressionResult.push_back(IeleRValue::Create(Return));
    break;
  }
  case FunctionType::Kind::ECMul: {
    // Visit arguments.
    llvm::SmallVector<iele::IeleValue *, 3> Arguments;
    llvm::SmallVector<iele::IeleLocalVariable *, 2> ReturnRegisters;
    iele::IeleLocalVariable *ResultX =
      iele::IeleLocalVariable::Create(&Context, "ecmul.x", CompilingFunction);
    iele::IeleLocalVariable *ResultY =
      iele::IeleLocalVariable::Create(&Context, "ecmul.y", CompilingFunction);
    ReturnRegisters.push_back(ResultX);
    ReturnRegisters.push_back(ResultY);

    IeleRValue *ArgValue = compileExpression(*arguments[0]);
    solAssert(ArgValue,
              "IeleCompiler: Failed to compile internal function call "
              "argument");
    // Check if we need to do a memory to/from storage copy.
    TypePointer ArgType = arguments[0]->annotation().type;
    TypePointer ParamType = function.parameterTypes()[0];
    auto arrayType = dynamic_cast<const ArrayType *>(ParamType);
    ArgValue = appendTypeConversion(ArgValue, ArgType, ParamType);
    iele::IeleValue *x = appendArrayAccess(*arrayType, iele::IeleIntConstant::getZero(&Context),
        ArgValue->getValue(), arrayType->location())->read(CompilingBlock)->getValue();
    iele::IeleValue *y = appendArrayAccess(*arrayType, iele::IeleIntConstant::getOne(&Context),
        ArgValue->getValue(), arrayType->location())->read(CompilingBlock)->getValue();
    Arguments.push_back(x);
    Arguments.push_back(y);

    ArgValue = compileExpression(*arguments[1]);
    solAssert(ArgValue,
              "IeleCompiler: Failed to compile internal function call "
              "argument");
    // Check if we need to do a memory to/from storage copy.
    ArgType = arguments[1]->annotation().type;
    ParamType = function.parameterTypes()[1];
    ArgValue = appendTypeConversion(ArgValue, ArgType, ParamType);
    Arguments.push_back(ArgValue->getValue());

    IeleRValue *CalleeValue = compileExpression(functionCall.expression());
    iele::IeleGlobalVariable *FunctionCalleeValue =
      iele::IeleGlobalVariable::Create(&Context, "iele.ecmul");
    iele::IeleValue *AddressValue =
      iele::IeleIntConstant::getOne(&Context);

    iele::IeleLocalVariable *StatusValue =
      CompilingFunctionStatus;

    iele::IeleValue *GasValue;
    if (!function.gasSet()) {
      llvm::SmallVector<iele::IeleValue *, 0> EmptyArguments;
      iele::IeleLocalVariable *GasLeft =
        iele::IeleLocalVariable::Create(&Context, "gas", CompilingFunction);
      iele::IeleInstruction::CreateIntrinsicCall(
        iele::IeleInstruction::Gas, GasLeft, EmptyArguments,
        CompilingBlock);
      GasValue = GasLeft;
    } else {
      GasValue = CalleeValue->getValues()[0];
    }

    iele::IeleInstruction::CreateAccountCall(
      true, StatusValue, ReturnRegisters, FunctionCalleeValue, AddressValue,
      nullptr, GasValue, Arguments, CompilingBlock);

    appendRevert(StatusValue, StatusValue);

    auto returnType = dynamic_cast<const ArrayType *>(function.returnParameterTypes()[0]);
    iele::IeleValue *Return = appendArrayAllocation(*returnType);
    appendArrayAccess(*returnType, iele::IeleIntConstant::getZero(&Context),
      Return, returnType->location())->write(IeleRValue::Create(ReturnRegisters[0]), CompilingBlock);
    appendArrayAccess(*returnType, iele::IeleIntConstant::getOne(&Context),
      Return, returnType->location())->write(IeleRValue::Create(ReturnRegisters[1]), CompilingBlock);
    CompilingExpressionResult.push_back(IeleRValue::Create(Return));
    break;
  }
  case FunctionType::Kind::ECPairing: {
    // Visit arguments.
    llvm::SmallVector<iele::IeleValue *, 2> Arguments;
    llvm::SmallVector<iele::IeleLocalVariable *, 1> ReturnRegisters;
    iele::IeleLocalVariable *Result =
      iele::IeleLocalVariable::Create(&Context, "ecpairing.result", CompilingFunction);
    ReturnRegisters.push_back(Result);


    llvm::SmallVector<iele::IeleValue *, 2> Lengths;
    for (unsigned i = 0; i < arguments.size(); ++i) {
      IeleRValue *ArgValue = compileExpression(*arguments[i]);
      solAssert(ArgValue,
                "IeleCompiler: Failed to compile internal function call "
                "argument");
      // Check if we need to do a memory to/from storage copy.
      TypePointer ArgType = arguments[i]->annotation().type;
      TypePointer ParamType = function.parameterTypes()[i];
      ArgValue = appendTypeConversion(ArgValue, ArgType, ParamType);

      iele::IeleLocalVariable *Length =
        iele::IeleLocalVariable::Create(&Context, std::string("ecpairing.g") + char('1'+i) + ".length",
        CompilingFunction);
      Lengths.push_back(Length);
      iele::IeleInstruction::CreateLoad(
        Length, ArgValue->getValue(), CompilingBlock);
      iele::IeleLocalVariable *NextFree = appendMemorySpill();
      llvm::SmallVector<IeleRValue *, 1> Args;
      Args.push_back(ArgValue);
      TypePointers types;
      types.push_back(ParamType);
      iele::IeleValue *ByteWidth = encoding(Args, types, NextFree, false);
      iele::IeleLocalVariable *EncodedVal = 
        iele::IeleLocalVariable::Create(&Context, std::string("ecpairing.g") + char('1'+i), CompilingFunction);
      iele::IeleInstruction::CreateLoad(
        EncodedVal, NextFree,
        iele::IeleIntConstant::getZero(&Context),
        ByteWidth, CompilingBlock);
      Arguments.push_back(EncodedVal);
    }
    solAssert(Lengths.size() == 2, "invalid number of arguments");
    iele::IeleLocalVariable *Condition =
      iele::IeleLocalVariable::Create(&Context, "have.invalid.lengths", CompilingFunction);
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::CmpNe, Condition, Lengths[0], Lengths[1],
      CompilingBlock);
    appendRevert(Condition);
    Arguments.insert(Arguments.begin(), Lengths[0]);

    IeleRValue *CalleeValue = compileExpression(functionCall.expression());
    iele::IeleGlobalVariable *FunctionCalleeValue =
      iele::IeleGlobalVariable::Create(&Context, "iele.ecpairing");
    iele::IeleValue *AddressValue =
      iele::IeleIntConstant::getOne(&Context);

    iele::IeleLocalVariable *StatusValue =
      CompilingFunctionStatus;

    iele::IeleValue *GasValue;
    if (!function.gasSet()) {
      llvm::SmallVector<iele::IeleValue *, 0> EmptyArguments;
      iele::IeleLocalVariable *GasLeft =
        iele::IeleLocalVariable::Create(&Context, "gas", CompilingFunction);
      iele::IeleInstruction::CreateIntrinsicCall(
        iele::IeleInstruction::Gas, GasLeft, EmptyArguments,
        CompilingBlock);
      GasValue = GasLeft;
    } else {
      GasValue = CalleeValue->getValues()[0];
    }

    iele::IeleInstruction::CreateAccountCall(
      true, StatusValue, ReturnRegisters, FunctionCalleeValue, AddressValue,
      nullptr, GasValue, Arguments, CompilingBlock);

    appendRevert(StatusValue, StatusValue);

    CompilingExpressionResult.push_back(IeleRValue::Create(Result));
    break;
  }
  case FunctionType::Kind::ECRecover: {
    // Visit arguments.
    llvm::SmallVector<iele::IeleValue *, 4> Arguments;
    llvm::SmallVector<iele::IeleLocalVariable *, 4> ReturnRegisters;
    llvm::SmallVector<IeleLValue *, 4> Returns;
    compileFunctionArguments(Arguments, ReturnRegisters, Returns, arguments, function, true);

    IeleRValue *CalleeValue = compileExpression(functionCall.expression());
    iele::IeleGlobalVariable *FunctionCalleeValue =
      iele::IeleGlobalVariable::Create(&Context, "iele.ecrec");
    iele::IeleValue *AddressValue =
      iele::IeleIntConstant::getOne(&Context);

    iele::IeleLocalVariable *StatusValue =
      CompilingFunctionStatus;

    iele::IeleValue *GasValue;
    if (!function.gasSet()) {
      llvm::SmallVector<iele::IeleValue *, 0> EmptyArguments;
      iele::IeleLocalVariable *GasLeft =
        iele::IeleLocalVariable::Create(&Context, "gas", CompilingFunction);
      iele::IeleInstruction::CreateIntrinsicCall(
        iele::IeleInstruction::Gas, GasLeft, EmptyArguments,
        CompilingBlock);
      GasValue = GasLeft;
    } else {
      GasValue = CalleeValue->getValues()[0];
    }

    iele::IeleInstruction::CreateAccountCall(
      true, StatusValue, ReturnRegisters, FunctionCalleeValue, AddressValue,
      nullptr, GasValue, Arguments, CompilingBlock);

    appendRevert(StatusValue, StatusValue);
    iele::IeleLocalVariable *Failed =
      iele::IeleLocalVariable::Create(&Context, "ecrec.failed", CompilingFunction);
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::CmpEq, Failed, ReturnRegisters[0],
      iele::IeleIntConstant::getMinusOne(&Context),
      CompilingBlock);
    appendRevert(Failed);

    CompilingExpressionResult.insert(
        CompilingExpressionResult.end(), Returns.begin(), Returns.end());
    break;
  }
  case FunctionType::Kind::BlockHash: {
    IeleRValue *BlockNum = compileExpression(*arguments.front());
    BlockNum = appendTypeConversion(BlockNum, arguments.front()->annotation().type, function.parameterTypes()[0]);
    llvm::SmallVector<iele::IeleValue *, 0> Arguments(1, BlockNum->getValue());
    iele::IeleLocalVariable *BlockHashValue =
      iele::IeleLocalVariable::Create(&Context, "blockhash", CompilingFunction);
    iele::IeleInstruction::CreateIntrinsicCall(
      iele::IeleInstruction::Blockhash, BlockHashValue, Arguments,
      CompilingBlock);
    CompilingExpressionResult.push_back(IeleRValue::Create(BlockHashValue));
    break;
  }
  case FunctionType::Kind::KECCAK256:
  case FunctionType::Kind::SHA256:
  case FunctionType::Kind::RIPEMD160: {
    // Visit arguments
    llvm::SmallVector<IeleRValue *, 4> Arguments;
    TypePointers Types;
    for (unsigned i = 0; i < arguments.size(); ++i) {
      IeleRValue *ArgValue = compileExpression(*arguments[i]);
      solAssert(ArgValue,
                "IeleCompiler: Failed to compile internal function call "
                "argument");
      // Check if we need to do a memory to/from storage copy.
      TypePointer ArgType = arguments[i]->annotation().type;
      ArgValue = appendTypeConversion(ArgValue, ArgType, ArgType->mobileType());
      ArgType = ArgType->mobileType();
      Arguments.push_back(ArgValue);
      Types.push_back(ArgType);
    }

    llvm::SmallVector<iele::IeleLocalVariable *, 1> Returns;
    iele::IeleLocalVariable *Return =
      iele::IeleLocalVariable::Create(&Context, "computed.hash", CompilingFunction);
    Returns.push_back(Return);

    // Allocate cell 
    iele::IeleLocalVariable *NextFree = appendMemorySpill();
    iele::IeleValue *ByteWidth = encoding(Arguments, Types, NextFree, false);
    if (function.kind() == FunctionType::Kind::KECCAK256) {
      iele::IeleInstruction::CreateSha3(
        Return, NextFree, CompilingBlock);
    } else {
      iele::IeleLocalVariable *ArgValue =
        iele::IeleLocalVariable::Create(&Context, "encoded.val", CompilingFunction);
      iele::IeleInstruction::CreateLoad(
        ArgValue, NextFree, iele::IeleIntConstant::getZero(&Context), ByteWidth, CompilingBlock);
      iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::BSwap,
        ArgValue, ByteWidth, ArgValue,
        CompilingBlock);
      Arguments.clear();
      llvm::SmallVector<iele::IeleValue *, 2> ArgumentRegisters;
      ArgumentRegisters.push_back(ByteWidth);
      ArgumentRegisters.push_back(ArgValue);

      IeleRValue *CalleeValue = compileExpression(functionCall.expression());

      iele::IeleGlobalVariable *FunctionCalleeValue;
      if (function.kind() == FunctionType::Kind::SHA256) {
        FunctionCalleeValue = iele::IeleGlobalVariable::Create(&Context, "iele.sha256");
      } else {
        FunctionCalleeValue = iele::IeleGlobalVariable::Create(&Context, "iele.rip160");
      }

      iele::IeleValue *AddressValue =
        iele::IeleIntConstant::getOne(&Context);

      iele::IeleLocalVariable *StatusValue =
        CompilingFunctionStatus;
 
      iele::IeleValue *GasValue;
      if (!function.gasSet()) {
        llvm::SmallVector<iele::IeleValue *, 0> EmptyArguments;
        iele::IeleLocalVariable *GasLeft =
          iele::IeleLocalVariable::Create(&Context, "gas", CompilingFunction);
        iele::IeleInstruction::CreateIntrinsicCall(
          iele::IeleInstruction::Gas, GasLeft, EmptyArguments,
          CompilingBlock);
        GasValue = GasLeft;
      } else {
        GasValue = CalleeValue->getValues()[2];
      }

      iele::IeleInstruction::CreateAccountCall(
        true, StatusValue, Returns, FunctionCalleeValue, AddressValue,
        nullptr, GasValue, ArgumentRegisters, CompilingBlock);

      appendRevert(StatusValue, StatusValue);
    }
    CompilingExpressionResult.push_back(IeleRValue::Create(Return));
    break;
  }
  case FunctionType::Kind::ByteArrayPush: {
    IeleRValue *PushedValue =
      arguments.size() ? compileExpression(*arguments.front()) : nullptr;
    iele::IeleValue *ArrayValue = compileExpression(functionCall.expression())->getValue();
    IeleLValue *SizeLValue = AddressLValue::Create(this, ArrayValue, CompilingLValueArrayType->location());
    iele::IeleValue *SizeValue = SizeLValue->read(CompilingBlock)->getValue();
    iele::IeleLocalVariable *StringAddress =
      iele::IeleLocalVariable::Create(&Context, "string.address", CompilingFunction);
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Add, StringAddress, ArrayValue,
      iele::IeleIntConstant::getOne(&Context),
      CompilingBlock);
    IeleLValue *ElementLValue = ByteArrayLValue::Create(this, StringAddress, SizeValue, CompilingLValueArrayType->location());
    if (PushedValue) {
      ElementLValue->write(PushedValue, CompilingBlock);
    }

    iele::IeleLocalVariable *NewSize =
      iele::IeleLocalVariable::Create(&Context, "array.new.length", CompilingFunction);
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Add, NewSize, SizeValue,
      iele::IeleIntConstant::getOne(&Context),
      CompilingBlock);
    IeleRValue *rvalue = IeleRValue::Create(NewSize);
    SizeLValue->write(rvalue, CompilingBlock);

    if (!PushedValue) {
      CompilingExpressionResult.push_back(ElementLValue);
    }
    break;
  }
  case FunctionType::Kind::ArrayPush: {
    IeleRValue *PushedValue =
      arguments.size() ? compileExpression(*arguments.front()) : nullptr;
    iele::IeleValue *ArrayValue = compileExpression(functionCall.expression())->getValue();
    IeleLValue *SizeLValue = AddressLValue::Create(this, ArrayValue, CompilingLValueArrayType->location());
    iele::IeleValue *SizeValue = SizeLValue->read(CompilingBlock)->getValue();

    TypePointer elementType = CompilingLValueArrayType->baseType();

    bigint elementSize;
    switch (CompilingLValueArrayType->location()) {
    case DataLocation::Storage: {
      elementSize = elementType->storageSize();
      break;
    }
    case DataLocation::CallData:
    case DataLocation::Memory: {
      elementSize = elementType->memorySize();
      break;
    }
    }
 
    iele::IeleLocalVariable *AddressValue =
      iele::IeleLocalVariable::Create(&Context, "array.end.address", CompilingFunction);
    // compute the data offset of the end of the array
    appendMul(AddressValue, SizeValue, elementSize);
    // add one for the length slot
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Add, AddressValue, AddressValue,
      iele::IeleIntConstant::getOne(&Context),
      CompilingBlock);
    // compute the address of the last element
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Add, AddressValue, AddressValue, ArrayValue,
      CompilingBlock);

    // new element lvalue
    IeleLValue *ElementLValue =
      makeLValue(AddressValue, elementType, CompilingLValueArrayType->location());

    // write the pushed value (if available)
    if (PushedValue) {
      TypePointer RHSType = arguments.front()->annotation().type;
      PushedValue = appendTypeConversion(PushedValue, RHSType, elementType);
      RHSType = RHSType->mobileType();
      if (shouldCopyStorageToStorage(*elementType, ElementLValue, *RHSType))
        appendCopyFromStorageToStorage(ElementLValue, elementType, PushedValue, RHSType);
      else if (shouldCopyMemoryToStorage(*elementType, ElementLValue, *RHSType))
        appendCopyFromMemoryToStorage(ElementLValue, elementType, PushedValue, RHSType);
      else if (shouldCopyMemoryToMemory(*elementType, ElementLValue, *RHSType))
        appendCopyFromMemoryToMemory(ElementLValue, elementType, PushedValue, RHSType);
      else
        ElementLValue->write(PushedValue, CompilingBlock);
    }

    // update array length
    iele::IeleLocalVariable *NewSize =
      iele::IeleLocalVariable::Create(&Context, "array.new.length", CompilingFunction);
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Add, NewSize, SizeValue,
      iele::IeleIntConstant::getOne(&Context),
      CompilingBlock);
    IeleRValue *rvalue = IeleRValue::Create(NewSize);
    SizeLValue->write(rvalue, CompilingBlock);

    if (!PushedValue) {
      CompilingExpressionResult.push_back(ElementLValue);
    }
    break;
  }
  case FunctionType::Kind::ArrayPop: {
    iele::IeleValue *ArrayValue = compileExpression(functionCall.expression())->getValue();
    IeleLValue *SizeLValue = AddressLValue::Create(this, ArrayValue, CompilingLValueArrayType->location());
    iele::IeleValue *SizeValue = SizeLValue->read(CompilingBlock)->getValue();

    TypePointer elementType = CompilingLValueArrayType->baseType();

    bigint elementSize;
    switch (CompilingLValueArrayType->location()) {
    case DataLocation::Storage: {
      elementSize = elementType->storageSize();
      break;
    }
    case DataLocation::CallData:
    case DataLocation::Memory: {
      elementSize = elementType->memorySize();
      break;
    }
    }

    iele::IeleLocalVariable *PopEmpty =
      iele::IeleLocalVariable::Create(&Context, "pop.empty", CompilingFunction);
    iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::CmpLe, PopEmpty, SizeValue,
        iele::IeleIntConstant::getZero(&Context), CompilingBlock);
    appendRevert(PopEmpty);

    iele::IeleLocalVariable *AddressValue =
      iele::IeleLocalVariable::Create(&Context, "array.last.element", CompilingFunction);
    // compute the data offset of the last element
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Sub, AddressValue, SizeValue,
      iele::IeleIntConstant::getOne(&Context),
      CompilingBlock);
    appendMul(AddressValue, AddressValue, elementSize);
    // add one for the length slot
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Add, AddressValue, AddressValue,
      iele::IeleIntConstant::getOne(&Context),
      CompilingBlock);
    // compute the address of the last element
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Add, AddressValue, AddressValue, ArrayValue,
      CompilingBlock);

    // delete the last element
    appendLValueDelete(
      makeLValue(AddressValue, elementType, CompilingLValueArrayType->location()),
      elementType);

    // update array length
    iele::IeleLocalVariable *NewSize =
      iele::IeleLocalVariable::Create(&Context, "array.new.length", CompilingFunction);
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Sub, NewSize, SizeValue,
      iele::IeleIntConstant::getOne(&Context),
      CompilingBlock);
    IeleRValue *rvalue = IeleRValue::Create(NewSize);
    SizeLValue->write(rvalue, CompilingBlock);

    break;
  }
  case FunctionType::Kind::ABIEncode:
  case FunctionType::Kind::ABIEncodePacked: {
    llvm::SmallVector<IeleRValue *, 4> Arguments;
    TypePointers Types;
    for (unsigned i = 0; i < arguments.size(); ++i) {
      IeleRValue *ArgValue = compileExpression(*arguments[i]);
      solAssert(ArgValue,
                "IeleCompiler: Failed to compile internal function call "
                "argument");
      // Check if we need to do a memory to/from storage copy.
      TypePointer ArgType = arguments[i]->annotation().type;
      ArgValue = appendTypeConversion(ArgValue, ArgType, ArgType->mobileType());
      ArgType = ArgType->mobileType();
      Arguments.push_back(ArgValue);
      Types.push_back(ArgType);
    }

    TypePointer returnType = function.returnParameterTypes()[0];
    iele::IeleValue *Return = appendArrayAllocation(dynamic_cast<const ArrayType &>(*returnType));

    iele::IeleLocalVariable *StringAddress =
      iele::IeleLocalVariable::Create(&Context, "string.address", CompilingFunction);
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Add, StringAddress, Return,
      iele::IeleIntConstant::getOne(&Context),
      CompilingBlock);

    iele::IeleValue *ByteWidth = encoding(Arguments, Types, StringAddress, function.kind() == FunctionType::Kind::ABIEncode);
    
    iele::IeleInstruction::CreateStore(
      ByteWidth, Return, CompilingBlock);

    CompilingExpressionResult.push_back(IeleRValue::Create(Return));
    break;
  }
  case FunctionType::Kind::ABIDecode: {
    IeleRValue *ArgValue = compileExpression(*arguments.front());
    TypePointer ArgType = arguments.front()->annotation().type;
    ArgValue = appendTypeConversion(ArgValue, ArgType, TypeProvider::bytesMemory());

    TypePointers TargetTypes;
    if (TupleType const* TargetTupleType =
          dynamic_cast<TupleType const*>(functionCall.annotation().type))
      TargetTypes = TargetTupleType->components();
    else
      TargetTypes = TypePointers{functionCall.annotation().type};

    llvm::SmallVector<IeleRValue *, 4> DecodedValues;
    decoding(ArgValue, TargetTypes, DecodedValues);
    CompilingExpressionResult.insert(
      CompilingExpressionResult.end(), DecodedValues.begin(), DecodedValues.end());
    break;
  }
  case FunctionType::Kind::MetaType:
    CompilingExpressionResult.push_back(IeleRValue::Create({}));
    break;
  default:
      solAssert(false, "IeleCompiler: Invalid function type.");
  }

  return false;
}


bool IeleCompiler::visit(const FunctionCallOptions &functionCallOptions) {
  IeleRValue *CalleeValue = compileExpression(functionCallOptions.expression());
  std::vector<iele::IeleValue *> values;
  values.insert(values.end(), CalleeValue->getValues().begin(),
                CalleeValue->getValues().end());
  FunctionTypePointer functionType =
    dynamic_cast<FunctionTypePointer>(
                 functionCallOptions.expression().annotation().type);

  int numOptions = functionCallOptions.options().size() ;
  solAssert(numOptions <= 2, "");

  for (int i = 0; i < numOptions; i++) {
    if (functionCallOptions.names()[i]->compare("gas") == 0) {
      IeleRValue *OptionValue =
        compileExpression(*functionCallOptions.options()[i]);
      OptionValue =
        appendTypeConversion(OptionValue,
                             functionCallOptions.options()[i]->annotation().type,
                             UInt);
      values.insert(values.begin() + functionType->gasIndex(), OptionValue->getValue());
    } else if (functionCallOptions.names()[i]->compare("value") == 0) {
      IeleRValue *OptionValue =
        compileExpression(*functionCallOptions.options()[i]);
      OptionValue =
        appendTypeConversion(OptionValue,
                             functionCallOptions.options()[i]->annotation().type,
                             UInt);
      values.insert(values.begin() + functionType->valueIndex(), OptionValue->getValue());
    } else {
      solAssert(false, "not implemented yet");
    }
  }

  CompilingExpressionResult.push_back(IeleRValue::Create(values));
  return false;
} 

template <typename ExpressionClass>
void IeleCompiler::compileFunctionArguments(
      llvm::SmallVectorImpl<iele::IeleValue *> &Arguments,
      llvm::SmallVectorImpl<iele::IeleLocalVariable *> &ReturnRegisters, 
      llvm::SmallVectorImpl<IeleLValue *> &Returns, 
      const std::vector<ASTPointer<ExpressionClass>> &arguments,
      const FunctionType &function, bool encode) {
    for (unsigned i = 0; i < arguments.size(); ++i) {
      IeleRValue *ArgValue = compileExpression(*arguments[i]);
      solAssert(ArgValue,
                "IeleCompiler: Failed to compile internal function call "
                "argument");
      // Check if we need to do a memory to/from storage copy.
      TypePointer ArgType = arguments[i]->annotation().type;
      TypePointer ParamType = function.parameterTypes()[i];
      ArgValue = appendTypeConversion(ArgValue, ArgType, ParamType);
      if (encode) {
        ArgValue = encoding(ArgValue, ParamType);
      }
      Arguments.insert(Arguments.end(), ArgValue->getValues().begin(), ArgValue->getValues().end());
    }

    // Prepare registers for return values.
    for (TypePointer type : function.returnParameterTypes()) {
      std::vector<iele::IeleLocalVariable *> param;
      for (unsigned j = 0; j < type->sizeInRegisters(); j++) {
        auto reg = iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
        ReturnRegisters.push_back(reg);
        param.push_back(reg);
      }
      Returns.push_back(RegisterLValue::Create(param));
    }
}

bool IeleCompiler::visit(const NewExpression &newExpression) {
  const Type &Type = *newExpression.typeName().annotation().type;
  const ContractType *contractType = dynamic_cast<const ContractType *>(&Type);
  solAssert(contractType, "not implemented yet");
  const ContractDefinition &ContractDefinition = contractType->contractDefinition();
  auto ContractCompilerIter = OtherCompilers.find(&ContractDefinition);
  solAssert(ContractCompilerIter != OtherCompilers.end(),
            "Could not find compiled contract for new expression");
  iele::IeleContract *Contract = &ContractCompilerIter->second->assembly();
  std::vector<iele::IeleContract *> &contracts = CompilingContract->getIeleContractList();
  if (std::find(contracts.begin(), contracts.end(), Contract) == contracts.end()) {
    CompilingContract->getIeleContractList().push_back(Contract);
  }
  CompilingExpressionResult.push_back(IeleRValue::Create(Contract));
  return false;
}

void IeleCompiler::appendGasLeft() {
  llvm::SmallVector<iele::IeleValue *, 0> EmptyArguments;
  iele::IeleLocalVariable *GasValue =
    iele::IeleLocalVariable::Create(&Context, "gas", CompilingFunction);
  iele::IeleInstruction::CreateIntrinsicCall(
    iele::IeleInstruction::Gas, GasValue, EmptyArguments,
    CompilingBlock);
  CompilingExpressionResult.push_back(IeleRValue::Create(GasValue));
}


bool IeleCompiler::visit(const MemberAccess &memberAccess) {
   const std::string& member = memberAccess.memberName();

  // Not supported yet cases.
  if (const FunctionType *funcType =
        dynamic_cast<const FunctionType *>(
            memberAccess.annotation().type)) {
    if (funcType->bound()) {
      IeleRValue *boundValue = compileExpression(memberAccess.expression());
      boundValue = appendTypeConversion(boundValue, 
        memberAccess.expression().annotation().type,
        funcType->selfType());
      std::vector<iele::IeleValue *> Result;
      Result.insert(Result.end(), boundValue->getValues().begin(), boundValue->getValues().end());
      
      const Declaration *declaration =
        memberAccess.annotation().referencedDeclaration;
      const FunctionDefinition *functionDef =
            dynamic_cast<const FunctionDefinition *>(declaration);
      solAssert(functionDef, "IeleCompiler: bound function does not have a definition");
      std::string name = getIeleNameForFunction(*functionDef); 

      // Lookup identifier in the compiling function's variable name map.
      if (IeleLValue *Identifier =
            VarNameMap[ModifierDepth][name]) {
        IeleRValue *RValue = Identifier->read(CompilingBlock);
        Result.insert(Result.begin(), RValue->getValues().begin(), RValue->getValues().end());
        CompilingExpressionResult.push_back(IeleRValue::Create(Result));
        return false;
      }
      iele::IeleValueSymbolTable *ST = CompilingContract->getIeleValueSymbolTable();
      solAssert(ST,
               "IeleCompiler: failed to access compiling contract's symbol "
               "table.");
      if (iele::IeleValue *Identifier = ST->lookup(name)) {
        IeleLValue *var = appendGlobalVariable(Identifier, functionDef->name(), true, 1);
        IeleRValue *RValue = var->read(CompilingBlock);
        Result.insert(Result.begin(), RValue->getValues().begin(), RValue->getValues().end());
        CompilingExpressionResult.push_back(IeleRValue::Create(Result));
        return false;
      }

      solAssert(false, "Invalid bound function without declaration");
      return false;
    }
  }

  const Type *actualType = memberAccess.expression().annotation().type;
  if (const TypeType *type = dynamic_cast<const TypeType *>(actualType)) {
    if (dynamic_cast<const ContractType *>(type->actualType())) {
      iele::IeleValueSymbolTable *ST = CompilingContract->getIeleValueSymbolTable();
      solAssert(ST,
                "IeleCompiler: failed to access compiling contract's symbol table.");
      if (auto funType = dynamic_cast<const FunctionType *>(memberAccess.annotation().type)) {
        switch(funType->kind()) {
        case FunctionType::Kind::DelegateCall:
        case FunctionType::Kind::Internal:
	  if (const auto * function = dynamic_cast<const FunctionDefinition *>(memberAccess.annotation().referencedDeclaration)) {
            std::string name = getIeleNameForFunction(*function);
            iele::IeleValue *Result = ST->lookup(name);
            CompilingExpressionResult.push_back(IeleRValue::Create(Result));
            return false;
          } else {
            solAssert(false, "Function member not found");
          }
        case FunctionType::Kind::External:
        case FunctionType::Kind::Creation:
        case FunctionType::Kind::Send:
        case FunctionType::Kind::Transfer:
        case FunctionType::Kind::Declaration:
          // handled below
          actualType = type->actualType();
          break;
        case FunctionType::Kind::BareCall:
        case FunctionType::Kind::BareCallCode:
        case FunctionType::Kind::BareDelegateCall:
        case FunctionType::Kind::BareStaticCall:
        default:
          solAssert(false, "not implemented yet");
        }
      } else if (dynamic_cast<const TypeType *>(memberAccess.annotation().type)) {
        return false;
        //noop
      } else if (auto variable = dynamic_cast<const VariableDeclaration *>(memberAccess.annotation().referencedDeclaration)) {
        if (variable->isConstant()) {
          IeleRValue *Result = compileExpression(*variable->value());
          TypePointer rhsType = variable->value()->annotation().type;
          Result = appendTypeConversion(Result, rhsType, variable->annotation().type);
          CompilingExpressionResult.push_back(Result);
        } else {
          std::string name = getIeleNameForStateVariable(variable);
          iele::IeleValue *Result = ST->lookup(name);
          solAssert(Result, "IeleCompiler: failed to find state variable in "
                            "contract's symbol table");
          CompilingExpressionResult.push_back(
            appendGlobalVariable(Result, variable->name(), variable->annotation().type->isValueType(), variable->annotation().type->sizeInRegisters()));
        }
        return false;
      } else {
        solAssert(false, "not implemented yet");
      }
    } else if (auto enumType = dynamic_cast<const EnumType *>(type->actualType())) {
      iele::IeleIntConstant *Result = iele::IeleIntConstant::Create(&Context, bigint(enumType->memberValue(memberAccess.memberName())));
      CompilingExpressionResult.push_back(IeleRValue::Create(Result));
      return false;
    } else {
      solAssert(false, "not implemented yet");
    }
  }

  if (auto type = dynamic_cast<const ContractType *>(actualType)) {
 
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
      CompilingExpressionResult.push_back(IeleRValue::Create(Result));
      return false;
    } else {
      if (const Declaration *declaration = memberAccess.annotation().referencedDeclaration) {
        IeleRValue *ContractValue = compileExpression(memberAccess.expression());
        std::string name;
        // don't call getIeleNameFor here because this is part of an external call and therefore is only able to
        // see the most-derived function
        if (const auto *variable = dynamic_cast<const VariableDeclaration *>(declaration)) {
          FunctionType accessorType(*variable);
          name = accessorType.externalSignature();
        } else if (const auto *function = dynamic_cast<const FunctionDefinition *>(declaration)) {
          name = function->externalSignature();
        } else {
          solAssert(false, "Contract member is neither variable nor function.");
        }
        std::vector<iele::IeleValue *> Values;
        iele::IeleGlobalValue *Function = iele::IeleFunction::Create(&Context, true, name);
        iele::IeleLocalVariable *Result =
          iele::IeleLocalVariable::Create(&Context, "function.pointer", CompilingFunction);
        iele::IeleInstruction::CreateCallAddress(
          Result, Function, ContractValue->getValue(),
          CompilingBlock);
        Values.push_back(ContractValue->getValue());
        Values.push_back(Result);
        CompilingExpressionResult.push_back(IeleRValue::Create(Values));
        return false;
      }
    }
  }

  const Type &baseType = *actualType;

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
      CompilingExpressionResult.push_back(IeleRValue::Create(TimestampValue));
    } else if (member == "difficulty") {
      llvm::SmallVector<iele::IeleValue *, 0> EmptyArguments;
      iele::IeleLocalVariable *DifficultyValue =
        iele::IeleLocalVariable::Create(&Context, "difficulty", CompilingFunction);
      iele::IeleInstruction::CreateIntrinsicCall(
        iele::IeleInstruction::Difficulty, DifficultyValue, EmptyArguments,
        CompilingBlock);
      CompilingExpressionResult.push_back(IeleRValue::Create(DifficultyValue));
    } else if (member == "number") {
      llvm::SmallVector<iele::IeleValue *, 0> EmptyArguments;
      iele::IeleLocalVariable *NumberValue =
        iele::IeleLocalVariable::Create(&Context, "number", CompilingFunction);
      iele::IeleInstruction::CreateIntrinsicCall(
        iele::IeleInstruction::Number, NumberValue, EmptyArguments,
        CompilingBlock);
      CompilingExpressionResult.push_back(IeleRValue::Create(NumberValue));
    } else if (member == "gaslimit") {
      llvm::SmallVector<iele::IeleValue *, 0> EmptyArguments;
      iele::IeleLocalVariable *GaslimitValue =
        iele::IeleLocalVariable::Create(&Context, "gaslimit", CompilingFunction);
      iele::IeleInstruction::CreateIntrinsicCall(
        iele::IeleInstruction::Gaslimit, GaslimitValue, EmptyArguments,
        CompilingBlock);
      CompilingExpressionResult.push_back(IeleRValue::Create(GaslimitValue));
    } else if (member == "sender") {
      llvm::SmallVector<iele::IeleValue *, 0> EmptyArguments;
      iele::IeleLocalVariable *CallerValue =
      iele::IeleLocalVariable::Create(&Context, "caller", CompilingFunction);
      iele::IeleInstruction::CreateIntrinsicCall(
        iele::IeleInstruction::Caller, CallerValue, EmptyArguments,
        CompilingBlock);
      CompilingExpressionResult.push_back(IeleRValue::Create(CallerValue));
    } else if (member == "value") {
      llvm::SmallVector<iele::IeleValue *, 0> EmptyArguments;
      iele::IeleLocalVariable *CallvalueValue =
        iele::IeleLocalVariable::Create(&Context, "callvalue", CompilingFunction);
      iele::IeleInstruction::CreateIntrinsicCall(
        iele::IeleInstruction::Callvalue, CallvalueValue, EmptyArguments,
        CompilingBlock);
      CompilingExpressionResult.push_back(IeleRValue::Create(CallvalueValue));
    } else if (member == "origin") {
      llvm::SmallVector<iele::IeleValue *, 0> EmptyArguments;
      iele::IeleLocalVariable *OriginValue =
        iele::IeleLocalVariable::Create(&Context, "origin", CompilingFunction);
      iele::IeleInstruction::CreateIntrinsicCall(
        iele::IeleInstruction::Origin, OriginValue, EmptyArguments,
        CompilingBlock);
      CompilingExpressionResult.push_back(IeleRValue::Create(OriginValue));
    } else if (member == "gas") {
      appendGasLeft();
    } else if (member == "gasprice") {
      llvm::SmallVector<iele::IeleValue *, 0> EmptyArguments;
      iele::IeleLocalVariable *GaspriceValue =
        iele::IeleLocalVariable::Create(&Context, "gasprice", CompilingFunction);
      iele::IeleInstruction::CreateIntrinsicCall(
        iele::IeleInstruction::Gasprice, GaspriceValue, EmptyArguments,
        CompilingBlock);
      CompilingExpressionResult.push_back(IeleRValue::Create(GaspriceValue));
    } else if (member == "coinbase") {
      llvm::SmallVector<iele::IeleValue *, 0> EmptyArguments;
      iele::IeleLocalVariable *BeneficiaryValue =
        iele::IeleLocalVariable::Create(&Context, "beneficiary", CompilingFunction);
      iele::IeleInstruction::CreateIntrinsicCall(
        iele::IeleInstruction::Beneficiary, BeneficiaryValue, EmptyArguments,
        CompilingBlock);
      CompilingExpressionResult.push_back(IeleRValue::Create(BeneficiaryValue));
    } else if (member == "min" || member == "max") {
      const MagicType *arg =
        dynamic_cast<const MagicType *>(actualType);
      const IntegerType *integerType =
        dynamic_cast<const IntegerType *>(arg->typeArgument());

      iele::IeleValue *MinMaxConstant =
        iele::IeleIntConstant::Create(&Context,
                                      member == "min" ? integerType->minValue()
                                                      : integerType->maxValue());
      iele::IeleLocalVariable *TypeSizeValue =
        iele::IeleLocalVariable::Create(&Context, "typesize", CompilingFunction);
      iele::IeleInstruction::CreateAssign(
          TypeSizeValue, MinMaxConstant, CompilingBlock);
      CompilingExpressionResult.push_back(IeleRValue::Create(TypeSizeValue));
    } else if (member == "name") {
      TypePointer arg = dynamic_cast<const MagicType *>(actualType)->typeArgument();
      auto const& contractType = dynamic_cast<ContractType const&>(*arg);
      ContractDefinition const& contract = contractType.isSuper() ?
        *superContract(dynamic_cast<const ContractDefinition &>(contractType.contractDefinition())) :
        contractType.contractDefinition();
      std::string contractName = contract.name();
      std::reverse(contractName.begin(), contractName.end());
      bigint contractName_integer = bigint(toHex(asBytes(contractName), HexPrefix::Add));
      iele::IeleIntConstant *ContractNameValue =
        iele::IeleIntConstant::Create(&Context, contractName_integer, true);
      IeleRValue *ConvertedValue =
        appendTypeConversion(IeleRValue::Create(ContractNameValue),
                             TypeProvider::stringLiteral(contractName),
                             memberAccess.annotation().type);
      CompilingExpressionResult.push_back(ConvertedValue);
    } else if (member == "interfaceId") {
      TypePointer arg = dynamic_cast<const MagicType *>(actualType)->typeArgument();
      ContractDefinition const& contract =
        dynamic_cast<ContractType const&>(*arg).contractDefinition();
      iele::IeleValue *InterfaceIDValue =
        iele::IeleIntConstant::Create(&Context, contract.interfaceId(), true);
      IeleRValue *ConvertedValue =
        appendTypeConversion(IeleRValue::Create(InterfaceIDValue),
                             TypeProvider::uint(32),
                             memberAccess.annotation().type);
      CompilingExpressionResult.push_back(ConvertedValue);
    } else if (member == "data")
      solAssert(false, "IeleCompiler: member not supported in IELE");
    else if (member == "sig")
      solAssert(false, "IeleCompiler: member not supported in IELE");
    else if (member == "blockhash")
      CompilingExpressionResult.push_back(IeleRValue::Create({}));
    else if (member == "encode")
      CompilingExpressionResult.push_back(IeleRValue::Create({}));
    else if (member == "encodePacked")
      CompilingExpressionResult.push_back(IeleRValue::Create({}));
    else if (member == "decode")
      CompilingExpressionResult.push_back(IeleRValue::Create({}));
    else if (member == "encodeWithSelector")
      solAssert(false, "IeleCompiler: member not supported in IELE");
    else if (member == "encodeWithSignature")
      solAssert(false, "IeleCompiler: member not supported in IELE");
    else
      solAssert(false, "IeleCompiler: Unknown magic member.");
    break;
  case Type::Category::Address: {
    // Visit accessed exression (skip in case of magic base expression).
    IeleRValue *ExprValue = compileExpression(memberAccess.expression());
    solAssert(ExprValue,
              "IeleCompiler: failed to compile base expression for member "
              "access.");
    if (member == "call" || member == "callcode" || member == "delegatecall" ||
        member == "staticcall") {
    // cannot be invoked in iele, so we don't need to create a result.
    } else if (member == "transfer" || member == "send") {
      ExprValue = appendTypeConversion(ExprValue,
        memberAccess.expression().annotation().type,
        Address);
      solAssert(ExprValue, "IeleCompiler: Failed to compile address");
      CompilingExpressionResult.push_back(ExprValue);
    } else if (member == "balance") {
      ExprValue = appendTypeConversion(ExprValue,
        memberAccess.expression().annotation().type,
        Address);
      llvm::SmallVector<iele::IeleValue *, 0> Arguments(1, ExprValue->getValue());
      iele::IeleLocalVariable *BalanceValue =
        iele::IeleLocalVariable::Create(&Context, "balance", CompilingFunction);
      iele::IeleInstruction::CreateIntrinsicCall(
        iele::IeleInstruction::Balance, BalanceValue, Arguments,
        CompilingBlock);
      CompilingExpressionResult.push_back(IeleRValue::Create(BalanceValue));
    } else if (member == "codesize") {
      ExprValue = appendTypeConversion(ExprValue,
        memberAccess.expression().annotation().type,
        Address);
      llvm::SmallVector<iele::IeleValue *, 0> Arguments(1, ExprValue->getValue());
      iele::IeleLocalVariable *CodeSizeValue =
        iele::IeleLocalVariable::Create(&Context, "codesize", CompilingFunction);
      iele::IeleInstruction::CreateIntrinsicCall(
        iele::IeleInstruction::Extcodesize, CodeSizeValue, Arguments,
        CompilingBlock);
      CompilingExpressionResult.push_back(IeleRValue::Create(CodeSizeValue));
    } else
      solAssert(false, "IeleCompiler: invalid member for integer value");
    break;
  }
  case Type::Category::Function:
    if (member == "value" || member == "gas") {
      IeleRValue *CalleeValue = compileExpression(memberAccess.expression());
      CompilingExpressionResult.push_back(CalleeValue);
    } else if (member == "address") {
      IeleRValue *FunctionValue = compileExpression(memberAccess.expression());
      IeleRValue *Result =
        appendTypeConversion(FunctionValue,
                             memberAccess.expression().annotation().type,
                             Address);
      CompilingExpressionResult.push_back(Result);
    } else if (member == "selector")
      solAssert(false, "IeleCompiler: member not supported in IELE");
    else
      solAssert(false, "IeleCompiler: invalid member for function value");
    break;
  case Type::Category::Struct: {
    const StructType &type = dynamic_cast<const StructType &>(baseType);

    // Visit accessed exression (skip in case of magic base expression).
    iele::IeleValue *ExprValue = compileExpression(memberAccess.expression())->getValue();
    solAssert(ExprValue,
              "IeleCompiler: failed to compile base expression for member "
              "access.");

    IeleLValue *AddressValue = appendStructAccess(type, ExprValue, member, type.location());
    CompilingExpressionResult.push_back(AddressValue);
    break;
  }
  case Type::Category::FixedBytes: {
    // Visit accessed exression (skip in case of magic base expression).
    iele::IeleValue *ExprValue = compileExpression(memberAccess.expression())->getValue();
    solAssert(ExprValue,
              "IeleCompiler: failed to compile base expression for member "
              "access.");

    const FixedBytesType &type = dynamic_cast<const FixedBytesType &>(baseType);
    if (member != "length")
      solAssert(false, "IeleCompiler: invalid member for bytesN value");
    iele::IeleValue *LengthValue =
      iele::IeleIntConstant::Create(
        &Context,
        bigint(type.numBytes()));
    CompilingExpressionResult.push_back(IeleRValue::Create(LengthValue));
    break;
  }
  case Type::Category::Array: {
    const ArrayType &type = dynamic_cast<const ArrayType &>(baseType);
    IeleRValue *ExprValue = compileExpression(memberAccess.expression());

    solAssert(ExprValue,
              "IeleCompiler: failed to compile base expression for member "
              "access.");

    const Type &elementType = *type.baseType();
    // Generate code for the access.
    bigint elementSize;
    switch (type.location()) {
    case DataLocation::Storage: {
      elementSize = elementType.storageSize();
      break;
    }
    case DataLocation::CallData:
    case DataLocation::Memory: {
      elementSize = elementType.memorySize();
      break;
    }
    }
 
    if (member == "length") {
      if (type.isDynamicallySized()) {
        // If the array is dynamically sized the size is stored in the first slot.
        CompilingExpressionResult.push_back(ArrayLengthLValue::Create(this, ExprValue->getValue(), type.location()));
        CompilingLValueArrayType = &type;
      } else {
        CompilingExpressionResult.push_back(IeleRValue::Create(
          iele::IeleIntConstant::Create(&Context, type.length())));
      }
    } else if (member == "push") {
      solAssert(type.isDynamicallySized(), "Invalid push on fixed length array");
      CompilingExpressionResult.push_back(ExprValue);
      CompilingLValueArrayType = &type;
      break;
    } else if (member == "pop") {
      solAssert(type.isDynamicallySized(), "Invalid pop on fixed length array");
      CompilingExpressionResult.push_back(ExprValue);
      CompilingLValueArrayType = &type;
      break;
    } else {
      solAssert(false, "not implemented yet");
    }
    break;
  }
  case Type::Category::Enum:
    solAssert(false, "not implemented yet: enums");
  default:
    solAssert(false, "IeleCompiler: Member access to unknown type.");
  }

  return false;
}

bool IeleCompiler::visit(const IndexAccess &indexAccess) {
  const Type &baseType = *indexAccess.baseExpression().annotation().type;
  switch (baseType.category()) {
  case Type::Category::Array: {
    const ArrayType &type = dynamic_cast<const ArrayType &>(baseType);

    // Visit accessed exression.
    iele::IeleValue *ExprValue = compileExpression(indexAccess.baseExpression())->getValue();
    solAssert(ExprValue,
              "IeleCompiler: failed to compile base expression for index "
              "access.");

    // Visit index expression.
    solAssert(indexAccess.indexExpression(),
              "IeleCompiler: Index expression expected.");
    IeleRValue *IndexValue =
      compileExpression(*indexAccess.indexExpression());
    IndexValue =
      appendTypeConversion(
          IndexValue,
          indexAccess.indexExpression()->annotation().type, UInt);
    solAssert(IndexValue,
              "IeleCompiler: failed to compile index expression for index "
              "access.");

    IeleLValue *AddressValue = appendArrayAccess(type, IndexValue->getValue(), ExprValue, type.location());
    CompilingExpressionResult.push_back(AddressValue);
    break;
  }
  case Type::Category::FixedBytes: {
    const FixedBytesType &type = dynamic_cast<const FixedBytesType &>(baseType);

    // Visit accessed exression.
    iele::IeleValue *ExprValue = compileExpression(indexAccess.baseExpression())->getValue();
    solAssert(ExprValue,
              "IeleCompiler: failed to compile base expression for index "
              "access.");

    solAssert(indexAccess.indexExpression(),
              "IeleCompiler: Index expression expected.");

     // Visit index expression.
    IeleRValue *IndexValue =
      compileExpression(*indexAccess.indexExpression());
    IndexValue = appendTypeConversion(IndexValue, indexAccess.indexExpression()->annotation().type, UInt);
    solAssert(IndexValue,
              "IeleCompiler: failed to compile index expression for index "
              "access.");

    bigint min = 0;
    bigint max = type.numBytes() - 1;
    solAssert(max > 0, "IeleCompiler: found bytes0 type");
    appendRangeCheck(IndexValue->getValue(), &min, &max);

    iele::IeleLocalVariable *ByteValue =
      iele::IeleLocalVariable::Create(&Context, "tmp",
                                      CompilingFunction);
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Sub, ByteValue,
      iele::IeleIntConstant::Create(&Context, bigint(max)),
      IndexValue->getValue(), CompilingBlock);
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Byte, ByteValue, ByteValue, ExprValue, CompilingBlock);
    CompilingExpressionResult.push_back(IeleRValue::Create(ByteValue));
    break;
  }
  case Type::Category::Mapping: {
    const MappingType &type = dynamic_cast<const MappingType &>(baseType);
    TypePointer keyType = type.keyType();

    // Visit accessed exression.
    iele::IeleValue *ExprValue = compileExpression(indexAccess.baseExpression())->getValue();
    solAssert(ExprValue,
              "IeleCompiler: failed to compile base expression for index "
              "access.");

    // Visit index expression.
    solAssert(indexAccess.indexExpression(),
              "IeleCompiler: Index expression expected.");
    IeleRValue *IndexValue =
      compileExpression(*indexAccess.indexExpression());
    IndexValue =
      appendTypeConversion(
          IndexValue,
          indexAccess.indexExpression()->annotation().type, keyType);
    IndexValue = encoding(IndexValue, keyType);
    solAssert(IndexValue,
              "IeleCompiler: failed to compile index expression for index "
              "access.");
    IeleLValue *AddressValue = appendMappingAccess(type, IndexValue->getValue(), ExprValue);

    CompilingExpressionResult.push_back(AddressValue);
    break;
  }
  case Type::Category::TypeType:
    break;
  default:
    solAssert(false, "IeleCompiler: Index access to unknown type.");
  }

  return false;
}

IeleLValue *IeleCompiler::appendArrayAccess(const ArrayType &type, iele::IeleValue *IndexValue, iele::IeleValue *ExprValue, DataLocation Loc) {
  TypePointer elementType = type.baseType();

  // Generate code for the access.
  bigint elementSize;
  bigint size;
  switch (Loc) {
  case DataLocation::Storage: {
    elementSize = elementType->storageSize();
    size = type.storageSize();
    break;
  }
  case DataLocation::CallData:
  case DataLocation::Memory: {
    elementSize = elementType->memorySize();
    size = type.memorySize();
    break;
  }
  }

  // First compute the offset from the start of the array.
  iele::IeleLocalVariable *OffsetValue =
    iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
  if (!type.isByteArray()) {
    appendMul(OffsetValue, IndexValue, elementSize);
  }
  if (type.isDynamicallySized() && !type.isByteArray()) {
    // Add 1 to skip the first slot that holds the size.
    iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Add, OffsetValue, OffsetValue,
        iele::IeleIntConstant::getOne(&Context), CompilingBlock);
  }
  // Then compute the size of the array.
  iele::IeleValue *SizeValue = nullptr;
  if (type.isDynamicallySized()) {
    // If the array is dynamically sized the size is stored in the first slot.
    SizeValue =
      iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
    if (Loc == DataLocation::Storage)
      iele::IeleInstruction::CreateSLoad(
          llvm::cast<iele::IeleLocalVariable>(SizeValue), ExprValue,
          CompilingBlock);
    else
      iele::IeleInstruction::CreateLoad(
          llvm::cast<iele::IeleLocalVariable>(SizeValue), ExprValue,
          CompilingBlock);
  } else {
    bigint sizeInElements = size / elementSize;
    SizeValue = iele::IeleIntConstant::Create(&Context, sizeInElements);
  }
  // Then check for out-of-bounds access.
  iele::IeleLocalVariable *OutOfRangeValue =
    iele::IeleLocalVariable::Create(&Context, "index.out.of.range",
                                    CompilingFunction);
  iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::CmpLt, OutOfRangeValue, IndexValue, 
      iele::IeleIntConstant::getZero(&Context), CompilingBlock);
  appendRevert(OutOfRangeValue);
  iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::CmpGe, OutOfRangeValue, IndexValue, 
      SizeValue, CompilingBlock);
  appendRevert(OutOfRangeValue);
  // Then compute the address of the accessed element and check for
  // out-of-bounds access.
  iele::IeleLocalVariable *AddressValue =
    iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
  if (type.isByteArray()) {
    iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Add, AddressValue, ExprValue, iele::IeleIntConstant::getOne(&Context),
        CompilingBlock);
  } else {
    iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Add, AddressValue, ExprValue, OffsetValue,
        CompilingBlock);
  }

  return makeLValue(AddressValue, elementType, Loc, type.isByteArray() ? IndexValue : nullptr);
}

IeleLValue *IeleCompiler::appendMappingAccess(const MappingType &type, iele::IeleValue *IndexValue, iele::IeleValue *ExprValue) {
  TypePointer valueType = type.valueType();
  // Hash index if needed.
  if (type.hasHashedKeyspace()) {
    iele::IeleLocalVariable *MemorySpillAddress = appendMemorySpill();
    iele::IeleInstruction::CreateStore(
        IndexValue, MemorySpillAddress, CompilingBlock);
    iele::IeleLocalVariable *HashedIndexValue =
      iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
    iele::IeleInstruction::CreateSha3(
        HashedIndexValue, MemorySpillAddress, CompilingBlock);
    IndexValue = HashedIndexValue;
  }

  // Generate code for the access.
  // First compute the address of the accessed value.
  iele::IeleLocalVariable *AddressValue =
    iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
  if (type.hasInfiniteKeyspace()) {
    // In this case AddressValue = ExprValue + IndexValue < nbits(StorageSize)
    appendShiftBy(AddressValue, IndexValue,
                  util::bitsRequired(NextStorageAddress));
    iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Add, AddressValue, ExprValue, AddressValue,
        CompilingBlock);
  } else {
    // In this case AddressValue = ExprValue + IndexValue * ValueTypeSize
    appendMul(AddressValue, IndexValue, valueType->storageSize());
    iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Add, AddressValue, ExprValue, AddressValue,
        CompilingBlock);
  }
  return makeLValue(AddressValue, valueType, DataLocation::Storage);
}

IeleLValue *IeleCompiler::appendStructAccess(const StructType &type, iele::IeleValue *ExprValue, std::string member, DataLocation Loc) {
  const TypePointer memberType = type.memberType(member);

  // Generate code for the access.
  // First compute the offset from the start of the struct.
  bigint offset;
  switch (Loc) {
  case DataLocation::Storage: {
    const auto& offsets = type.storageOffsetsOfMember(member);
    offset = offsets.first;
    break;
  }
  case DataLocation::CallData:
  case DataLocation::Memory: {
    offset = type.memoryOffsetOfMember(member);
    break;
  }
  }
  iele::IeleValue *OffsetValue =
    iele::IeleIntConstant::Create(&Context, offset);
  // Then compute the address of the accessed element.
  iele::IeleLocalVariable *AddressValue =
    iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
  iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Add, AddressValue, ExprValue, OffsetValue,
      CompilingBlock);
  return makeLValue(AddressValue, memberType, Loc);
}

void IeleCompiler::appendRangeCheck(iele::IeleValue *Value, bigint *min, bigint *max) {
    if (iele::IeleIntConstant *constant = llvm::dyn_cast<iele::IeleIntConstant>(Value)) {
      if ((min && constant->getValue() < *min) || (max && constant->getValue() > *max)) {
        appendRevert();
      }
      return;
    }
    iele::IeleLocalVariable *OutOfRangeValue =
      iele::IeleLocalVariable::Create(&Context, "out.of.range",
                                      CompilingFunction);
    if (min) {
      iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::CmpLt, OutOfRangeValue, Value, 
        iele::IeleIntConstant::Create(&Context, *min),
        CompilingBlock);
      appendRevert(OutOfRangeValue);
    }

    if (max) {
      iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::CmpGt, OutOfRangeValue, Value,
        iele::IeleIntConstant::Create(&Context, *max),
        CompilingBlock);
      appendRevert(OutOfRangeValue);
    }
}

void IeleCompiler::appendRangeCheck(IeleRValue *Value, const Type &Type) {
  bigint min, max;
  switch(Type.category()) {
  case Type::Category::Integer: {
    const IntegerType &type = dynamic_cast<const IntegerType &>(Type);
    if (type.isUnbound() && type.isSigned()) {
      return;
    } else if (type.isUnbound()) {
      bigint min = 0;
      if (iele::IeleIntConstant *constant = llvm::dyn_cast<iele::IeleIntConstant>(Value->getValue())) {
        if (constant->getValue() < 0) {
          appendRevert();
        }
      } else {
        appendRangeCheck(Value->getValue(), &min, nullptr);
      }
      return;
    } else if (type.isSigned()) {
      min = -(bigint(1) << (type.numBits() - 1));
      max = (bigint(1) << (type.numBits() - 1)) - 1;
    } else {
      min = 0;
      max = (bigint(1) << type.numBits()) - 1;
    }
    break;
  }
  case Type::Category::Address: {
    min = 0;
    max = (bigint(1) << 160) - 1;
    break;
  }
  case Type::Category::Bool: {
    min = 0;
    max = 1;
    break;
  }
  case Type::Category::FixedBytes: {
    const FixedBytesType &type = dynamic_cast<const FixedBytesType &>(Type);
    min = 0;
    max = (bigint(1) << (type.numBytes() * 8)) - 1;
    break;
  }
  case Type::Category::Contract: {
    min = 0;
    max = (bigint(1) << 160) - 1;
    break;
  }
  case Type::Category::Enum: {
    const EnumType &type = dynamic_cast<const EnumType &>(Type);
    solAssert(type.numberOfMembers() > 0, "Invalid empty enum");
    min = 0;
    max = type.numberOfMembers() - 1;
    break;
  }
  case Type::Category::Function: {
    const FunctionType &functionType = dynamic_cast<const FunctionType &>(Type);
    FunctionType::Kind kind;
    if (functionType.kind() == FunctionType::Kind::SetGas || functionType.kind() == FunctionType::Kind::SetValue)
    {
      solAssert(functionType.returnParameterTypes().size() == 1, "");
      kind = dynamic_cast<FunctionType const&>(*functionType.returnParameterTypes().front()).kind();
    } else {
      kind = functionType.kind();
    }
    unsigned index = 0;
    min = 0;
    switch (kind) {
    case FunctionType::Kind::External:
      max = (bigint(1) << 160) - 1;
      appendRangeCheck(Value->getValues()[0], &min, &max);
      max = (bigint(1) << 16) - 1;
      appendRangeCheck(Value->getValues()[1], &min, &max);
      index = 2;
      break;
    case FunctionType::Kind::BareCall:
    case FunctionType::Kind::BareCallCode:
    case FunctionType::Kind::BareDelegateCall:
    case FunctionType::Kind::BareStaticCall:
    case FunctionType::Kind::Internal:
    case FunctionType::Kind::DelegateCall:
      max = (bigint(1) << 16) - 1;
      appendRangeCheck(Value->getValues()[0], &min, &max);
      index = 1;
      break;
    case FunctionType::Kind::ArrayPush:
    case FunctionType::Kind::ByteArrayPush:
    case FunctionType::Kind::ArrayPop:
      solAssert(false, "not implemented yet");
    default:
      break;
    }
    if (functionType.gasSet()) {
      bigint min = 0;
      if (iele::IeleIntConstant *constant = llvm::dyn_cast<iele::IeleIntConstant>(Value->getValue())) {
        if (constant->getValue() < 0) {
          appendRevert();
        }
      } else {
        appendRangeCheck(Value->getValue(), &min, nullptr);
      }
      index++;
    }
    if (functionType.valueSet()) {
      bigint min = 0;
      if (iele::IeleIntConstant *constant = llvm::dyn_cast<iele::IeleIntConstant>(Value->getValue())) {
        if (constant->getValue() < 0) {
          appendRevert();
        }
      } else {
        appendRangeCheck(Value->getValue(), &min, nullptr);
      }
      index++;
    }
    if (functionType.bound()) {
      std::vector<iele::IeleValue *> boundValue;
      boundValue.insert(boundValue.end(), Value->getValues().begin() + index, Value->getValues().end());
      appendRangeCheck(IeleRValue::Create(boundValue), *functionType.selfType());
    }
    return;
  }
  default:
    solAssert(false, "not implemented yet");
  }
  appendRangeCheck(Value->getValue(), &min, &max);
}

bool IeleCompiler::visit(const ElementaryTypeNameExpression &typeName) {
  CompilingExpressionResult.push_back(IeleRValue::Create({}));
  return false;
}

void IeleCompiler::endVisit(const Identifier &identifier) {
  // Get the corrent name for the identifier.
  std::string name = identifier.name();
  const Declaration *declaration =
    identifier.annotation().referencedDeclaration;
  if (const FunctionDefinition *functionDef =
        dynamic_cast<const FunctionDefinition *>(declaration)) {
    if (contractFor(functionDef)->isLibrary()) {
      name = getIeleNameForFunction(*functionDef); 
    } else {
      name = getIeleNameForFunction(*resolveVirtualFunction(*functionDef)); 
    }
  } else if (const VariableDeclaration *varDecl =
               dynamic_cast<const VariableDeclaration *>(declaration)) {
    if (varDecl->isLocalVariable() && !varDecl->isCallableOrCatchParameter())
      name = getIeleNameForLocalVariable(varDecl);
  }

  // Check if identifier is a reserved identifier.
  if (const MagicVariableDeclaration *magicVar =
         dynamic_cast<const MagicVariableDeclaration *>(declaration)) {
    switch (magicVar->type()->category()) {
    case Type::Category::Contract: {
      llvm::SmallVector<iele::IeleValue *, 0> EmptyArguments;
      iele::IeleLocalVariable *This =
        iele::IeleLocalVariable::Create(&Context, "this", CompilingFunction);
      iele::IeleInstruction::CreateIntrinsicCall(iele::IeleInstruction::Address, This, EmptyArguments, CompilingBlock);
      CompilingExpressionResult.push_back(IeleRValue::Create(This));
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
      CompilingExpressionResult.push_back(IeleRValue::Create(TimestampValue));
      return;
    }
    default:
      // The rest of reserved identifiers appear only as part of a member access
      // expression and are handled there.
      CompilingExpressionResult.push_back(IeleRValue::Create({}));
      return;
    }
  }

  // Lookup identifier in the compiling function's variable name map.
  if (IeleLValue *Identifier =
        VarNameMap[ModifierDepth][name]) {
    CompilingExpressionResult.push_back(Identifier);
    return;
  }

  // If not found, lookup identifier in the contract's symbol table.
  bool isValueType = true;
  unsigned sizeInRegisters = 1;
  if (const VariableDeclaration *variableDecl =
        dynamic_cast<const VariableDeclaration *>(declaration)) {
    isValueType = variableDecl->annotation().type->isValueType();
    sizeInRegisters = variableDecl->annotation().type->sizeInRegisters();
    if (variableDecl->isConstant()) {
      IeleRValue *Identifier = compileExpression(*variableDecl->value());
      TypePointer rhsType = variableDecl->value()->annotation().type;
      Identifier = appendTypeConversion(Identifier, rhsType, variableDecl->annotation().type);
      CompilingExpressionResult.push_back(Identifier);
      return;
    }
  }
  iele::IeleValueSymbolTable *ST = CompilingContract->getIeleValueSymbolTable();
  solAssert(ST,
            "IeleCompiler: failed to access compiling contract's symbol "
            "table.");
  if (iele::IeleValue *Identifier = ST->lookup(name)) {
    CompilingExpressionResult.push_back(appendGlobalVariable(Identifier, identifier.name(), isValueType, sizeInRegisters));
    return;
  }

  // If not found, make a new IeleLocalVariable for the identifier.
  iele::IeleLocalVariable *Identifier =
    iele::IeleLocalVariable::Create(&Context, name, CompilingFunction);
  CompilingExpressionResult.push_back(RegisterLValue::Create({Identifier}));
  return;
}

IeleLValue *IeleCompiler::appendGlobalVariable(iele::IeleValue *Identifier, std::string name,
                                  bool isValueType, unsigned sizeInRegisters) {
    if (llvm::isa<iele::IeleGlobalVariable>(Identifier)) {
      if (isValueType) {
        // In case of a global variable, if we aren't compiling an lvalue and
        // the global variable holds a value type, we have to load the global
        // variable.
        return AddressLValue::Create(this, Identifier, DataLocation::Storage, name, sizeInRegisters);
      } else {
        return ReadOnlyLValue::Create(IeleRValue::Create(Identifier));
      }
    } else {
      return ReadOnlyLValue::Create(IeleRValue::Create(Identifier));
    }
}

void IeleCompiler::endVisit(const Literal &literal) {
  TypePointer type = literal.annotation().type;
  switch (type->category()) {
  case Type::Category::Bool:
  case Type::Category::RationalNumber:
  case Type::Category::Address:
  case Type::Category::Integer: {
    iele::IeleIntConstant *LiteralValue =
      iele::IeleIntConstant::Create(
          &Context,
          type->literalValue(&literal));
    CompilingExpressionResult.push_back(IeleRValue::Create(LiteralValue));
    break;
  }
  case Type::Category::StringLiteral: {
    const auto &literalType = dynamic_cast<const StringLiteralType &>(*type);
    std::string value = literalType.value();
    std::reverse(value.begin(), value.end());
    bigint value_integer = bigint(toHex(asBytes(value), HexPrefix::Add));
    iele::IeleIntConstant *LiteralValue =
      iele::IeleIntConstant::Create(
        &Context,
        value_integer,
        true);
    CompilingExpressionResult.push_back(IeleRValue::Create(LiteralValue));
    break;
  }
  default:
    solAssert(false, "not implemented yet");
    break;
  }
}

IeleRValue *IeleCompiler::compileExpression(const Expression &expression) {
  // Save current expression compilation status.
  bool SavedCompilingLValue = CompilingLValue;

  // Visit expression.
  CompilingLValue = false;
  expression.accept(*this);

  // Expression visitors should store the value that is the result of the
  // compiled for the expression computation in the CompilingExpressionResult
  // field. This helper should only be used when a scalar value (or void) is
  // expected as the result of the corresponding expression computation.
  IeleRValue *Result = nullptr;
  solAssert(CompilingExpressionResult.size() == 1,
            "IeleCompiler: Expression visitor did not set enough result values");
  Result = CompilingExpressionResult[0].rval(CompilingBlock);

  // Restore expression compilation status and return result.
  CompilingExpressionResult.clear();
  CompilingLValue = SavedCompilingLValue;
  return Result;
}

void IeleCompiler::compileTuple(
    const Expression &expression,
    llvm::SmallVectorImpl<IeleRValue *> &Result) {
  // Save current expression compilation status.
  bool SavedCompilingLValue = CompilingLValue;

  // Visit expression.
  CompilingLValue = false;
  expression.accept(*this);

  // Expression visitors should store the value that is the result of the
  // compiled for the expression computation in the CompilingExpressionResult
  // field. This helper should only be used when tupple value is expected as
  // the result of the corresponding expression computation.
  for (Value val : CompilingExpressionResult) {
    Result.push_back(val.rval(CompilingBlock));
  }

  // Restore expression compilation status and return result.
  CompilingExpressionResult.clear();
  CompilingLValue = SavedCompilingLValue;
}

IeleLValue *IeleCompiler::compileLValue(const Expression &expression) {
  // Save current expression compilation status.
  bool SavedCompilingLValue = CompilingLValue;

  // Visit expression as LValue.
  CompilingLValue = true;
  expression.accept(*this);

  // Expression visitors should store the value that is the result of the
  // compiled for the expression computation in the CompilingExpressionResult
  // field. This helper should only be used when a scalar lvalue is expected as
  // the result of the corresponding expression computation.
  IeleLValue *Result = nullptr;
  solAssert(CompilingExpressionResult.size() == 1,
            "IeleCompiler: Expression visitor did not set a result value");
  Result = CompilingExpressionResult[0].lval();

  // Restore expression compilation status and return result.
  CompilingExpressionResult.clear();
  CompilingLValue = SavedCompilingLValue;
  return Result;
}

void IeleCompiler::compileLValues(
    const Expression &expression,
    llvm::SmallVectorImpl<IeleLValue *> &Result) {
  // Save current expression compilation status.
  bool SavedCompilingLValue = CompilingLValue;

  // Visit expression as LValue.
  CompilingLValue = true;
  expression.accept(*this);

  // Expression visitors should store the value that is the result of the
  // compiled for the expression computation in the CompilingExpressionResult
  // field. This helper should only be used when a scalar lvalue is expected as
  // the result of the corresponding expression computation.
  solAssert(CompilingExpressionResult.size() > 0, "expression visitor did not set a result value");
  for (Value val : CompilingExpressionResult) {
    Result.push_back(val.lval());
  }

  // Restore expression compilation status and return result.
  CompilingExpressionResult.clear();
  CompilingLValue = SavedCompilingLValue;
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
    if (Status != CompilingFunctionStatus) {
      iele::IeleInstruction::CreateAssign(
          CompilingFunctionStatus, Status, CompilingBlock);
    }
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
    llvm::SmallVector<iele::IeleValue *, 0> EmptyArguments;
    iele::IeleInstruction::CreateIntrinsicCall(
        iele::IeleInstruction::Invalid, nullptr,
        EmptyArguments, AssertFailBlock);
  }

  if (Condition)
    connectWithConditionalJump(Condition, CompilingBlock, AssertFailBlock);
  else
    connectWithUnconditionalJump(CompilingBlock, AssertFailBlock);
}

void IeleCompiler::computeCtorsAuxParams() {
  // Consider all contracts in hierarchy, starting from bottom
  for (const ContractDefinition *def : CompilingContractInheritanceHierarchy) {
    // For each one, consider all base contracts
    for (const auto &base : def->baseContracts()) {
      auto baseContract = dynamic_cast<const ContractDefinition *>(
        base->name().annotation().referencedDeclaration);
      solAssert(baseContract, 
                "Must find base contract in inheritance specifier");
      // Add aux param to all contracts which are between def and basecontract
      // in the inheritance chain...
      bool found = false; 
      for (const ContractDefinition *it : CompilingContractInheritanceHierarchy) {
        
        if (it == def) {
          found = true;
          continue;
        }

        if (it == baseContract)
          break;

        if (found) {
          if (ctorAuxParams[it][baseContract].first.size() == 0) {
            std::vector<std::string> auxParamNames;

            auto baseArgumentNode = CompilingContractInheritanceHierarchy[0]->annotation().baseConstructorArguments[baseContract->constructor()];
            std::vector<ASTPointer<Expression>> const* arguments = nullptr;
            if (auto inheritanceSpecifier = dynamic_cast<InheritanceSpecifier const*>(baseArgumentNode))
               arguments = inheritanceSpecifier->arguments();
             else if (auto modifierInvocation = dynamic_cast<ModifierInvocation const*>(baseArgumentNode))
               arguments = modifierInvocation->arguments();

            if (arguments) {
              for (unsigned i = 0; i < arguments->size(); i++) {
                std::string newParamName = "aux" + getNextVarSuffix();
                auxParamNames.push_back(newParamName);
              }
            }

            ctorAuxParams[it][baseContract] = std::make_pair(auxParamNames, def);
          }
        } 
      }    
    }  
  }
}

void IeleCompiler::appendDefaultConstructor(const ContractDefinition *contract) {
  // Init state variables.
  bool found = false;
  for (const ContractDefinition *def : CompilingContractInheritanceHierarchy) {
    if (found) {
      iele::IeleValueSymbolTable *ST = CompilingContract->getIeleValueSymbolTable();
      solAssert(ST,
                "IeleCompiler: failed to access compiling function's symbol "
                "table.");
      
      iele::IeleValue *ConstructorValue = ST->lookup(getIeleNameForContract(def) + ".init");
      solAssert(ConstructorValue, "IeleCompiler: failed to find constructor for contract " + getIeleNameForContract(def));
      iele::IeleGlobalValue *CalleeValue =
        llvm::dyn_cast<iele::IeleGlobalValue>(ConstructorValue);

      // Call the immediate base class init function.
      llvm::SmallVector<iele::IeleValue *, 4> Arguments;
      llvm::SmallVector<iele::IeleLocalVariable *, 0> ReturnRegisters;
      llvm::SmallVector<IeleLValue *, 0> Returns;

      // TODO: can we assume NumOfModifiers to be always zero here? 
      unsigned NumOfModifiers;
      if (CompilingFunctionASTNode)
        NumOfModifiers = CompilingFunctionASTNode->modifiers().size();
      else  
        NumOfModifiers = 0;
      
      iele::IeleValueSymbolTable *FunST =
          CompilingFunction->getIeleValueSymbolTable();
        solAssert(FunST,
          "IeleCompiler: failed to access compiling function's symbol "
          "table.");
      
      // Shall we compute and pass a value to base ctor, or just "forward" 
      // one of our aux parameters instead? 
      if (const FunctionDefinition *decl = def->constructor()) {
        std::pair<std::vector<std::string>, 
                  const ContractDefinition *> forwardingAuxParams = 
          ctorAuxParams[contract][def];

        // No aux params to forward, compute value "normally"
        if (forwardingAuxParams.first.empty()) { 
          auto baseArgumentNode = CompilingContractInheritanceHierarchy[0]->annotation().baseConstructorArguments[decl];
          std::vector<ASTPointer<Expression>> const* arguments = nullptr;
          if (auto inheritanceSpecifier = dynamic_cast<InheritanceSpecifier const*>(baseArgumentNode))
            arguments = inheritanceSpecifier->arguments();
          else if (auto modifierInvocation = dynamic_cast<ModifierInvocation const*>(baseArgumentNode))
            arguments = modifierInvocation->arguments();
          
          if (arguments) {
            const FunctionType &function = FunctionType(*decl);
            unsigned ModifierDepthCache = ModifierDepth;
            ModifierDepth = NumOfModifiers;
            compileFunctionArguments(Arguments, ReturnRegisters, Returns, *arguments, 
                                    function, false);
            ModifierDepth = ModifierDepthCache;
            solAssert(Returns.size() == 0, "Constructor doesn't return anything");
          }
        } else {
          // forward aux param to base constructor
          for (std::string auxParamName : forwardingAuxParams.first) {
            iele::IeleValue *AuxArg =
              VarNameMap[NumOfModifiers][auxParamName]->read(CompilingBlock)->getValue();
            solAssert(AuxArg, 
                      "IeleCompiler: missing local variable " + auxParamName);
            Arguments.push_back(AuxArg);
          }
        }
      }

      // Does the constructor being called have aux parameters?
      std::map<const ContractDefinition *, 
               std::pair<std::vector<std::string>, 
                         const ContractDefinition *>> baseCtorAuxParams = 
        ctorAuxParams[def];
      
      // If so, we need to handle them
      if (!baseCtorAuxParams.empty()) {
        // Iterate through aux parameters
        std::map<const ContractDefinition *, 
                 std::pair<std::vector<std::string>, 
                           const ContractDefinition *>>::iterator it;

        for(it = baseCtorAuxParams.begin(); it != baseCtorAuxParams.end(); ++it) {
          auto auxParamDest = it -> first;
          auto auxParams    = it -> second;
          auto paramNames   = auxParams.first;
          auto paramSource  = auxParams.second;

          // Aux param carries a value which is NOT to be evaluated in current
          // contract (the caller). Therefore, forward our own aux param. 
          if (paramSource != (contract)) {
            std::pair<std::vector<std::string>, 
                      const ContractDefinition *> p = 
              ctorAuxParams[contract][auxParamDest];

            if (!p.first.empty()) {
              for (std::string pName : p.first) {
                iele::IeleValue *AuxArg =
                  VarNameMap[NumOfModifiers][pName]->read(CompilingBlock)->getValue();
                solAssert(AuxArg, 
                          "IeleCompiler: missing local variable " + pName);
                Arguments.push_back(AuxArg);
              }
            }
          } else {
            // Base takes an aux param, carrying a value that needs to be 
            // evaluated in this contract. 
            auto decl = auxParamDest->constructor(); 
            auto baseArgumentNode = CompilingContractInheritanceHierarchy[0]->annotation().baseConstructorArguments[decl];
            std::vector<ASTPointer<Expression>> const* arguments = nullptr;
            if (auto inheritanceSpecifier = dynamic_cast<InheritanceSpecifier const*>(baseArgumentNode))
              arguments = inheritanceSpecifier->arguments();
            else if (auto modifierInvocation = dynamic_cast<ModifierInvocation const*>(baseArgumentNode))
              arguments = modifierInvocation->arguments();

            if (arguments) {
              if (arguments->size() > 0) {
                llvm::SmallVector<iele::IeleValue *, 4> AuxArguments;

                // Cache ModifierDepth
                unsigned ModifierDepthCache = ModifierDepth;
                ModifierDepth = NumOfModifiers;

                // compile args 
                for (unsigned i = 0; i < arguments->size(); ++i) {
                  IeleRValue *ArgValue = compileExpression(*(*arguments)[i]);
                  solAssert(ArgValue,
                            "IeleCompiler: Failed to compile internal function call "
                            "argument");
                  AuxArguments.insert(AuxArguments.end(), ArgValue->getValues().begin(), ArgValue->getValues().end());
                }

                // Restore ModifierDepth
                ModifierDepth = ModifierDepthCache;

                for (auto arg : AuxArguments) {
                  Arguments.push_back(arg);
                }
              }
            }
          }
        }
      }
      // Create call to base constructor
      iele::IeleInstruction::CreateInternalCall(ReturnRegisters, CalleeValue, Arguments, CompilingBlock);
      break;
    }

    if (def == contract) {
      found = true;
    }
  }
  // Init this contract's state variables.
  for (const VariableDeclaration *stateVariable :
         contract->stateVariables()) {
    TypePointer type = stateVariable->annotation().type;
    TypePointer rhsType;

    // Visit initialization value if it exists.
    IeleRValue *InitValue = nullptr;
    if (stateVariable->value()) {
      rhsType = stateVariable->value()->annotation().type;
      InitValue =
        appendTypeConversion(compileExpression(*stateVariable->value()),
                             rhsType, type);
    }

    if (!InitValue || stateVariable->isConstant())
      continue;

    // Lookup the state variable name in the contract's symbol table.
    iele::IeleValueSymbolTable *ST =
      CompilingContract->getIeleValueSymbolTable();
    solAssert(ST,
              "IeleCompiler: failed to access compiling contract's symbol "
              "table.");
    iele::IeleValue *LHSIeleValue =
      ST->lookup(getIeleNameForStateVariable(stateVariable));
    solAssert(LHSIeleValue, "IeleCompiler: Failed to compile LHS of state "
                            "variable initialization");
    IeleLValue *LHSValue = makeLValue(LHSIeleValue, type, DataLocation::Storage);

    if (type->isValueType()) {
      // Add assignment in constructor's body.
      LHSValue->write(InitValue, CompilingBlock);
    } else if (type->category() == Type::Category::Array ||
               type->category() == Type::Category::Struct) {
      // Add code for copy in constructor's body.
      rhsType = rhsType->mobileType();
      if (shouldCopyStorageToStorage(*type, LHSValue, *rhsType))
        appendCopyFromStorageToStorage(LHSValue, type, InitValue, rhsType);
      else if (shouldCopyMemoryToStorage(*type, LHSValue, *rhsType))
        appendCopyFromMemoryToStorage(LHSValue, type, InitValue, rhsType);
      else if (shouldCopyMemoryToMemory(*type, LHSValue, *rhsType))
        appendCopyFromMemoryToMemory(LHSValue, type, InitValue, rhsType);
    } else {
      solAssert(type->category() == Type::Category::Mapping,
                "IeleCompiler: found state variable initializer of unknown "
                "type");
      solAssert(false, "IeleCompiler: found state variable initializer for a "
                       "mapping");
    }
  }
}

void IeleCompiler::appendLocalVariableInitialization(
    IeleLValue *Local, const VariableDeclaration *localVariable) {
  iele::IeleValue *InitValue = nullptr;

  // Check if we need to allocate memory for a reference type.
  TypePointer type = localVariable->annotation().type;
  if (type->category() == Type::Category::Array) {
    const ArrayType &arrayType = dynamic_cast<const ArrayType &>(*type);
    if (arrayType.dataStoredIn(DataLocation::Memory)) {
      if (!arrayType.isDynamicallySized() || arrayType.isByteArray()) {
        InitValue = appendArrayAllocation(arrayType);
      }
    }
  } else if (type->category() == Type::Category::Struct) {
    const StructType &structType = dynamic_cast<const StructType &>(*type);
    if (structType.dataStoredIn(DataLocation::Memory))
      InitValue = appendStructAllocation(structType);
  }
  else {
    solAssert(type->isValueType() || type->category() == Type::Category::TypeType ||
              type->category() == Type::Category::Mapping,
              "IeleCompiler: found local variable of unknown type");
    // Local variables are always automatically initialized to zero. We don't
    // need to do anything here.
  }

  // Add assignment if needed.
  if (InitValue)
    Local->write(IeleRValue::Create(InitValue), CompilingBlock);
}

iele::IeleValue *IeleCompiler::getReferenceTypeSize(
    const Type &type, iele::IeleValue *AddressValue) {
  iele::IeleValue *SizeValue = nullptr;
  bool inStorage = type.dataStoredIn(DataLocation::Storage);
  switch (type.category()) {
  case (Type::Category::Array) : {
    const ArrayType &arrayType = dynamic_cast<const ArrayType &>(type);
    if (arrayType.isDynamicallySized() && !arrayType.isByteArray()) {
      // The size value can be found in the first slot of the array.
      iele::IeleLocalVariable *SizeVariable =
        iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
      SizeValue = SizeVariable;
      inStorage ?
        iele::IeleInstruction::CreateSLoad(
            SizeVariable, AddressValue,
            CompilingBlock) :
        iele::IeleInstruction::CreateLoad(
            SizeVariable, AddressValue,
            CompilingBlock) ;

      const Type &elementType = *arrayType.baseType();
      solAssert(!elementType.isDynamicallyEncoded(), "Cannot statically compute size of array of dynamic types");
      // Generate code for the access.
      bigint elementSize;
      switch (arrayType.location()) {
      case DataLocation::Storage: {
        elementSize = elementType.storageSize();
        break;
      }
      case DataLocation::CallData:
      case DataLocation::Memory: {
        elementSize = elementType.memorySize();
        break;
      }
      }
 
      appendMul(SizeVariable, SizeValue, elementSize);
      iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Add, SizeVariable, SizeValue,
        iele::IeleIntConstant::getOne(&Context),
        CompilingBlock);
    } else {
      SizeValue =
        iele::IeleIntConstant::Create(&Context, arrayType.memorySize());
    }
    break;
  }
  case (Type::Category::Struct) : {
    const StructType &structType = dynamic_cast<const StructType &>(type);
    solAssert(!structType.isDynamicallyEncoded(), "Cannot statically compute size of struct of dynamic types");
    SizeValue =
      iele::IeleIntConstant::Create(&Context, structType.memorySize());
    break;
  }
  case (Type::Category::Mapping) :
    solAssert(false, "IeleCompiler: requested size of mapping");
  default:
    solAssert(false, "IeleCompiler: invalid reference type");
  }

  return SizeValue;
}

iele::IeleLocalVariable *IeleCompiler::appendArrayAllocation(
    const ArrayType &type, iele::IeleValue *NumElemsValue) {
  const Type &elementType = *type.baseType();
  solAssert(elementType.category() != Type::Category::Mapping,
            "IeleCompiler: requested memory allocation of array of mappigns.");

  // Get allocation size in memory slots.
  iele::IeleValue *SizeValue = nullptr;
  if (NumElemsValue) {
    solAssert(type.isDynamicallySized() && !type.isByteArray(),
              "IeleCompiler: custom size requestd for fix-sized array");
    bigint elementSize;
    switch (type.location()) {
    case DataLocation::Storage: {
      elementSize = elementType.storageSize();
      break;
    }
    case DataLocation::CallData:
    case DataLocation::Memory: {
      elementSize = elementType.memorySize();
      break;
    }
    }

    SizeValue =
      iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
    appendMul(
        llvm::cast<iele::IeleLocalVariable>(SizeValue),
        NumElemsValue, elementSize);
    // Onr extra slot for storing length.
    iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Add,
        llvm::cast<iele::IeleLocalVariable>(SizeValue), SizeValue,
        iele::IeleIntConstant::getOne(&Context), CompilingBlock);
  } else {
    solAssert(!type.isDynamicallySized() || type.isByteArray(),
              "IeleCompiler: unknown size for dynamically-sized array");
    SizeValue = iele::IeleIntConstant::Create(&Context, type.memorySize());
  }

  // Call runtime for memory allocation.
  iele::IeleLocalVariable *ArrayAllocValue = appendIeleRuntimeAllocateMemory(SizeValue);

  // Save length for dynamically sized arrays.
  if (type.isDynamicallySized() && !type.isByteArray()) {
    iele::IeleInstruction::CreateStore(NumElemsValue, ArrayAllocValue,
                                       CompilingBlock);
  }

  return ArrayAllocValue;
}

iele::IeleLocalVariable *IeleCompiler::appendStructAllocation(const StructType &type) {
  // Get allocation size in memory slots.
  iele::IeleIntConstant *SizeValue =
    iele::IeleIntConstant::Create(&Context, type.memorySize());

  // Call runtime for memory allocation.
  iele::IeleLocalVariable *StructAllocValue = appendIeleRuntimeAllocateMemory(SizeValue);

  return StructAllocValue;
}

iele::IeleLocalVariable *IeleCompiler::appendRecursiveStructAllocation(
    const StructType &type) {
  // Get allocation size in memory slots.
  iele::IeleIntConstant *SizeValue =
    iele::IeleIntConstant::Create(&Context, type.storageSize());

  // Call runtime for storage allocation.
  iele::IeleLocalVariable *RecStructAllocValue =
    appendIeleRuntimeAllocateStorage(SizeValue);

  return RecStructAllocValue;
}

void IeleCompiler::appendCopy(
    IeleLValue *To, TypePointer ToType,
    IeleRValue *From, TypePointer FromType,
    DataLocation ToLoc, DataLocation FromLoc) {
  if (FromType->isValueType()) {
    solAssert(ToType->isValueType(), "invalid conversion");
    IeleRValue *AllocedValue = From;
    AllocedValue = appendTypeConversion(AllocedValue, FromType, ToType);
    To->write(AllocedValue, CompilingBlock);
    return;
  }
  iele::IeleValue *AllocedValue;

  if (!FromType->isDynamicallyEncoded() && *FromType == *ToType) {
    iele::IeleValue *SizeValue = getReferenceTypeSize(*FromType, From->getValue());
    if (dynamic_cast<RegisterLValue *>(To)) {
      AllocedValue = appendIeleRuntimeAllocateMemory(SizeValue);
      To->write(IeleRValue::Create(AllocedValue), CompilingBlock);
    } else {
      AllocedValue = To->read(CompilingBlock)->getValue();
    }
    appendIeleRuntimeCopy(From->getValue(), AllocedValue, SizeValue, FromLoc, ToLoc);
    return;
  }
  switch (FromType->category()) {
  case Type::Category::Struct: {
    const StructType &structType = dynamic_cast<const StructType &>(*FromType);
    const StructType &toStructType = dynamic_cast<const StructType &>(*ToType);

    if (dynamic_cast<RegisterLValue *>(To)) {
      AllocedValue = appendStructAllocation(toStructType);
      To->write(IeleRValue::Create(AllocedValue), CompilingBlock);
    } else {
      AllocedValue = To->read(CompilingBlock)->getValue();
    }

    if (structType.recursive()) {
      // Call the recursive copier.
      iele::IeleFunction *Copier =
        getRecursiveStructCopier(structType, FromLoc, toStructType, ToLoc);
      llvm::SmallVector<iele::IeleLocalVariable *, 0> EmptyResults;
      llvm::SmallVector<iele::IeleValue *, 2> Arguments;
      Arguments.push_back(From->getValue());
      Arguments.push_back(AllocedValue);
      iele::IeleInstruction::CreateInternalCall(
          EmptyResults, Copier, Arguments, CompilingBlock);
      break;
    }

    // Append the code for copying the struct members.
    appendStructCopy(structType, From->getValue(), FromLoc,
                     toStructType, AllocedValue, ToLoc);
    break;
  }
  case Type::Category::Array: {
    const ArrayType &arrayType = dynamic_cast<const ArrayType &>(*FromType);
    const ArrayType &toArrayType = dynamic_cast<const ArrayType &>(*ToType);
    TypePointer elementType = arrayType.baseType();
    TypePointer toElementType = toArrayType.baseType();
    if (arrayType.isByteArray() && toArrayType.isByteArray()) {
      // copy from bytes to string or back
      iele::IeleValue *SizeValue = getReferenceTypeSize(*FromType, From->getValue());
      if (dynamic_cast<RegisterLValue *>(To)) {
        AllocedValue = appendIeleRuntimeAllocateMemory(SizeValue);
        To->write(IeleRValue::Create(AllocedValue), CompilingBlock);
      } else {
        AllocedValue = To->read(CompilingBlock)->getValue();
      }
      appendIeleRuntimeCopy(From->getValue(), AllocedValue, SizeValue, FromLoc, ToLoc);
      return;
    }
    solAssert(!arrayType.isByteArray() && !toArrayType.isByteArray(), "not implemented yet");

    iele::IeleLocalVariable *SizeVariableFrom =
      iele::IeleLocalVariable::Create(&Context, "from.length", CompilingFunction);
    iele::IeleLocalVariable *SizeVariableTo =
      iele::IeleLocalVariable::Create(&Context, "to.length", CompilingFunction);

    iele::IeleLocalVariable *ElementFrom =
      iele::IeleLocalVariable::Create(&Context, "copy.from.address", CompilingFunction);

    iele::IeleLocalVariable *ElementTo =
      iele::IeleLocalVariable::Create(&Context, "copy.to.address", CompilingFunction);

    if (FromType->isDynamicallySized()) {
      FromLoc == DataLocation::Storage ?
        iele::IeleInstruction::CreateSLoad(
          SizeVariableFrom, From->getValue(),
          CompilingBlock) :
        iele::IeleInstruction::CreateLoad(
          SizeVariableFrom, From->getValue(),
          CompilingBlock) ;
      iele::IeleInstruction::CreateBinOp(
          iele::IeleInstruction::Add, ElementFrom, From->getValue(),
          iele::IeleIntConstant::getOne(&Context),
          CompilingBlock);
    } else {
      iele::IeleInstruction::CreateAssign(
        SizeVariableFrom, iele::IeleIntConstant::Create(&Context, bigint(arrayType.length())),
        CompilingBlock);
      iele::IeleInstruction::CreateAssign(
        ElementFrom, From->getValue(), CompilingBlock);
    }

    // copy the size field
    if (ToType->isDynamicallySized()) {
      if (ToLoc == DataLocation::Storage) {
        AllocedValue = To->read(CompilingBlock)->getValue();
        iele::IeleInstruction::CreateSLoad(
          SizeVariableTo, AllocedValue,
          CompilingBlock);
        iele::IeleInstruction::CreateSStore(
          SizeVariableFrom, AllocedValue,
          CompilingBlock);
      } else {
        iele::IeleInstruction::CreateAssign(
          SizeVariableTo, SizeVariableFrom,
          CompilingBlock);
        AllocedValue = appendArrayAllocation(toArrayType, SizeVariableTo);
        To->write(IeleRValue::Create(AllocedValue), CompilingBlock);
      }

      iele::IeleInstruction::CreateBinOp(
          iele::IeleInstruction::Add, ElementTo, AllocedValue,
          iele::IeleIntConstant::getOne(&Context),
          CompilingBlock);
    } else {
      if (dynamic_cast<RegisterLValue *>(To)) {
        AllocedValue = appendArrayAllocation(toArrayType);
        To->write(IeleRValue::Create(AllocedValue), CompilingBlock);
      } else {
        AllocedValue = To->read(CompilingBlock)->getValue();
      }
      iele::IeleInstruction::CreateAssign(
        SizeVariableTo, iele::IeleIntConstant::Create(&Context, bigint(toArrayType.length())),
        CompilingBlock);
      iele::IeleInstruction::CreateAssign(
        ElementTo, AllocedValue, CompilingBlock);
    }
 
    bigint elementSize, toElementSize;
    switch (FromLoc) {
    case DataLocation::Storage: {
      elementSize = elementType->storageSize();
      break;
    }
    case DataLocation::CallData:
    case DataLocation::Memory: {
      elementSize = elementType->memorySize();
      break;
    }
    }

    switch (ToLoc) {
    case DataLocation::Storage: {
      toElementSize = toElementType->storageSize();
      break;
    }
    case DataLocation::CallData:
    case DataLocation::Memory: {
      toElementSize = toElementType->memorySize();
      break;
    }
    }

    iele::IeleValue *FromElementSizeValue =
        iele::IeleIntConstant::Create(&Context, elementSize);
    iele::IeleValue *ToElementSizeValue =
        iele::IeleIntConstant::Create(&Context, toElementSize);

    iele::IeleLocalVariable *FillLoc, *FillSize;
    if (!elementType->isDynamicallyEncoded() && *elementType == *toElementType) {
      iele::IeleLocalVariable *CopySize =
        iele::IeleLocalVariable::Create(&Context, "copy.size", CompilingFunction);
      appendMul(CopySize, SizeVariableFrom, elementSize);
      appendIeleRuntimeCopy(ElementFrom, ElementTo, CopySize, FromLoc, ToLoc);

      FillLoc = iele::IeleLocalVariable::Create(&Context, "fill.address", CompilingFunction);
      iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Add, FillLoc, ElementTo, CopySize,
        CompilingBlock);

      FillSize = iele::IeleLocalVariable::Create(&Context, "fill.size", CompilingFunction);
      appendMul(FillSize, SizeVariableTo, toElementSize);
      iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Sub, FillSize, FillSize, CopySize,
        CompilingBlock);
    } else {
      iele::IeleBlock *LoopBodyBlock =
        iele::IeleBlock::Create(&Context, "copy.loop", CompilingFunction);
      CompilingBlock = LoopBodyBlock;
  
      iele::IeleBlock *LoopExitBlock =
        iele::IeleBlock::Create(&Context, "copy.loop.end");
  
      iele::IeleLocalVariable *DoneValue =
        iele::IeleLocalVariable::Create(&Context, "done", CompilingFunction);
      iele::IeleInstruction::CreateIsZero(
        DoneValue, SizeVariableFrom, CompilingBlock);
      connectWithConditionalJump(DoneValue, CompilingBlock, LoopExitBlock);
 
      appendCopy(makeLValue(ElementTo, toElementType, ToLoc), toElementType, makeLValue(ElementFrom, elementType, FromLoc)->read(CompilingBlock), elementType, ToLoc, FromLoc);
  
      iele::IeleInstruction::CreateBinOp(
          iele::IeleInstruction::Add, ElementFrom, ElementFrom,
          FromElementSizeValue,
          CompilingBlock);
      iele::IeleInstruction::CreateBinOp(
          iele::IeleInstruction::Add, ElementTo, ElementTo,
          ToElementSizeValue,
          CompilingBlock);
      iele::IeleInstruction::CreateBinOp(
          iele::IeleInstruction::Sub, SizeVariableFrom, SizeVariableFrom,
          iele::IeleIntConstant::getOne(&Context),
          CompilingBlock);
      iele::IeleInstruction::CreateBinOp(
          iele::IeleInstruction::Sub, SizeVariableTo, SizeVariableTo,
          iele::IeleIntConstant::getOne(&Context),
          CompilingBlock);
  
      connectWithUnconditionalJump(CompilingBlock, LoopBodyBlock);
      LoopExitBlock->insertInto(CompilingFunction);
      CompilingBlock = LoopExitBlock;

      FillLoc = ElementTo;
      FillSize = SizeVariableTo;
      appendMul(SizeVariableTo, SizeVariableTo, toElementSize);
    }

    iele::IeleLocalVariable *CondValue =
      iele::IeleLocalVariable::Create(&Context, "should.not.fill", CompilingFunction);
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::CmpLe, CondValue,
      FillSize, iele::IeleIntConstant::getZero(&Context), CompilingBlock);
  
    iele::IeleBlock *JoinBlock =
      iele::IeleBlock::Create(&Context, "if.end");
    connectWithConditionalJump(CondValue, CompilingBlock, JoinBlock);
    
    appendIeleRuntimeFill(FillLoc, FillSize,
      iele::IeleIntConstant::getZero(&Context),
      ToLoc);

    JoinBlock->insertInto(CompilingFunction);
    CompilingBlock = JoinBlock;

    break;
  }
  case Type::Category::Mapping: {
    solAssert(FromLoc == DataLocation::Storage,
              "IeleCompiler: mapping copy requested from memory");
    solAssert(ToLoc == DataLocation::Storage,
              "IeleCompiler: mapping copy requested to memory");
    break;
  }
  default:
    solAssert(false, "Invalid type in appendCopy");
  }
}

iele::IeleFunction *IeleCompiler::getRecursiveStructCopier(
    const StructType &type, DataLocation FromLoc,
    const StructType &toType, DataLocation ToLoc) {
  solAssert(type.recursive(),
            "IeleCompiler: attempted to construct recursive copier "
            "for non-recursive source struct type");
  solAssert(toType.recursive(),
            "IeleCompiler: attempted to construct recursive copier "
            "for non-recursive target struct type");

  std::string typeID = type.richIdentifier();
  std::string toTypeID = toType.richIdentifier();

  // Lookup copier in the cache.
  auto It = RecursiveStructCopiers.find(typeID);
  if (It != RecursiveStructCopiers.end()) {
    auto ItTo = It->second.find(toTypeID);
    if (ItTo != It->second.end())
      return ItTo->second;
  }

  // Generate a recursive function to copy a struct of the specific type.
  iele::IeleFunction *Copier =
    iele::IeleFunction::Create(
        &Context, false, "ielert.copy." + typeID + ".to." + toTypeID,
        CompilingContract);

  // Register copier function.
  RecursiveStructCopiers[typeID][toTypeID] = Copier;

  // Add the from/to address arguments.
  iele::IeleArgument *Address =
    iele::IeleArgument::Create(&Context, "from.addr", Copier);

  iele::IeleArgument *ToAddress =
    iele::IeleArgument::Create(&Context, "to.addr", Copier);

  // Add the first block.
  iele::IeleBlock *CopierBlock =
    iele::IeleBlock::Create(&Context, "entry", Copier);

  // Append the code for copying the struct members and returning.
  std::swap(CompilingFunction, Copier);
  std::swap(CompilingBlock, CopierBlock);
  appendStructCopy(type, Address, FromLoc, toType, ToAddress, ToLoc);
  iele::IeleInstruction::CreateRetVoid(CompilingBlock);
  std::swap(CompilingFunction, Copier);
  std::swap(CompilingBlock, CopierBlock);

  return Copier;
}

void IeleCompiler::appendStructCopy(
    const StructType &type, iele::IeleValue *Address, DataLocation FromLoc,
    const StructType &toType, iele::IeleValue *ToAddress, DataLocation ToLoc) {
  // We loop over the members of the source struct type.
  for (auto const &member : type.members(nullptr)) {
    // First find the types of the corresponding member in the source and
    // destination struct types.
    TypePointer memberFromType = member.type;
    TypePointer memberToType;
    for (auto const &toMember : toType.members(nullptr)) {
      if (member.name == toMember.name) {
        memberToType = toMember.type;
        break;
      }
    }

    // Ensure we are skipping mapping members when copying to memory.
    solAssert(!memberToType ||
              memberToType->category() != Type::Category::Mapping ||
              ToLoc == DataLocation::Storage,
              "IeleCompiler: found memory struct with mapping field");
    solAssert(memberFromType->category() != Type::Category::Mapping ||
              FromLoc == DataLocation::Storage,
              "IeleCompiler: found memory struct with mapping field");
    if (memberFromType->category() == Type::Category::Mapping &&
        ToLoc == DataLocation::Memory) {
      continue;
    }

    solAssert(memberToType, "IeleCompiler: could not find corresponding "
                            "non-mapping member in copy target struct type");

    // Then compute the offset from the start of the struct for the current
    // member. The offset may differ in the source and destination, due to
    // skipping of mappings in memory copies of structs.
    bigint fromOffset, toOffset;
    switch (FromLoc) {
    case DataLocation::Storage: {
      const auto& offsets = type.storageOffsetsOfMember(member.name);
      fromOffset = offsets.first;
      break;
    }
    case DataLocation::CallData:
    case DataLocation::Memory: {
      fromOffset = type.memoryOffsetOfMember(member.name);
      break;
    }
    }
    switch (ToLoc) {
    case DataLocation::Storage: {
      const auto& offsets = toType.storageOffsetsOfMember(member.name);
      toOffset = offsets.first;
      break;
    }
    case DataLocation::CallData:
    case DataLocation::Memory: {
      toOffset = toType.memoryOffsetOfMember(member.name);
      break;
    }
    }

    // Then compute the current member address in the source and destination
    // struct objects.
    iele::IeleLocalVariable *MemberFrom =
      iele::IeleLocalVariable::Create(&Context, "copy.from.address",
                                      CompilingFunction);
    iele::IeleValue *FromOffsetValue =
      iele::IeleIntConstant::Create(&Context, fromOffset);
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Add, MemberFrom, Address, FromOffsetValue,
      CompilingBlock);

    iele::IeleLocalVariable *MemberTo =
      iele::IeleLocalVariable::Create(&Context, "copy.to.address",
                                      CompilingFunction);
    iele::IeleValue *ToOffsetValue =
      iele::IeleIntConstant::Create(&Context, toOffset);
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Add, MemberTo, ToAddress, ToOffsetValue,
      CompilingBlock);

    // Finally do the copy of the member value from source to destination.
    appendCopy(
        makeLValue(MemberTo, memberToType, ToLoc), memberToType,
        makeLValue(MemberFrom, memberFromType, FromLoc)->read(CompilingBlock),
        memberFromType, ToLoc, FromLoc);
  }
}

void IeleCompiler::appendCopyFromStorageToStorage(
    IeleLValue *To, TypePointer ToType, IeleRValue *From, TypePointer FromType) {
  appendCopy(To, ToType, From, FromType, DataLocation::Storage, DataLocation::Storage);
}
void IeleCompiler::appendCopyFromMemoryToStorage(
    IeleLValue *To, TypePointer ToType, IeleRValue *From, TypePointer FromType) {
  appendCopy(To, ToType, From, FromType, DataLocation::Storage, DataLocation::Memory);
}
void IeleCompiler::appendCopyFromMemoryToMemory(
    IeleLValue *To, TypePointer ToType, IeleRValue *From, TypePointer FromType) {
  appendCopy(To, ToType, From, FromType, DataLocation::Memory, DataLocation::Memory);
}

iele::IeleValue *IeleCompiler::appendCopyFromStorageToMemory(
    TypePointer ToType, IeleRValue *From, TypePointer FromType) {
  iele::IeleLocalVariable *To =
    iele::IeleLocalVariable::Create(&Context, "copy.to", CompilingFunction);
  appendCopy(RegisterLValue::Create({To}), ToType, From, FromType, DataLocation::Memory, DataLocation::Storage);
  return To;
}

void IeleCompiler::appendArrayLengthResize(
    iele::IeleValue *LValue,
    iele::IeleValue *NewLength) {
  iele::IeleLocalVariable *OldLength =
    iele::IeleLocalVariable::Create(&Context, "old.length", CompilingFunction);
  iele::IeleInstruction::CreateSLoad(OldLength, LValue, CompilingBlock);

  iele::IeleLocalVariable *HasntShrunk =
    iele::IeleLocalVariable::Create(&Context, "has.not.shrunk", CompilingFunction);
  iele::IeleInstruction::CreateBinOp(
    iele::IeleInstruction::CmpGe, HasntShrunk, NewLength, OldLength,
    CompilingBlock);

  iele::IeleBlock *JoinBlock =
    iele::IeleBlock::Create(&Context, "if.end");
  connectWithConditionalJump(HasntShrunk, CompilingBlock, JoinBlock);

  if (CompilingLValueArrayType->isByteArray()) {
    iele::IeleLocalVariable *NewValue =
      iele::IeleLocalVariable::Create(&Context, "shrunk.value", CompilingFunction);
    iele::IeleLocalVariable *StringAddress =
      iele::IeleLocalVariable::Create(&Context, "string.address", CompilingFunction);
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Add, StringAddress, LValue,
      iele::IeleIntConstant::getOne(&Context),
      CompilingBlock);
    iele::IeleInstruction::CreateSLoad(NewValue, StringAddress, CompilingBlock);
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Twos,
      NewValue, NewLength, NewValue,
      CompilingBlock);
    iele::IeleInstruction::CreateSStore(NewValue, StringAddress, CompilingBlock);
  } else {
    const Type &elementType = *CompilingLValueArrayType->baseType();
    bigint elementSize = elementType.storageSize();
  
    iele::IeleLocalVariable *NewEnd =
      iele::IeleLocalVariable::Create(&Context, "new.end.of.array", CompilingFunction);
    appendMul(NewEnd, NewLength, elementSize);
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Add, NewEnd, NewEnd,
      LValue, CompilingBlock);
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Add, NewEnd, NewEnd,
      iele::IeleIntConstant::getOne(&Context),
      CompilingBlock);
    iele::IeleLocalVariable *DeleteLength =
      iele::IeleLocalVariable::Create(&Context, "length.to.delete", CompilingFunction);
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Sub, DeleteLength, OldLength, NewLength,
      CompilingBlock);
    appendArrayDeleteLoop(*CompilingLValueArrayType, NewEnd, DeleteLength);
  }

  JoinBlock->insertInto(CompilingFunction);
  CompilingBlock = JoinBlock;
}

iele::IeleValue *IeleCompiler::appendBooleanOperator(
    Token Opcode,
    const Expression &LeftOperand,
    const Expression &RightOperand) {
  solAssert(Opcode == Token::Or || Opcode == Token::And, "IeleCompiler: invalid boolean operator");

  iele::IeleValue *LeftOperandValue = 
    compileExpression(LeftOperand)->getValue();
  solAssert(LeftOperandValue, "IeleCompiler: Failed to compile left operand.");
  llvm::SmallVector<IeleLValue *, 1> Results;
  if (Opcode == Token::Or) {
    appendConditional(
      LeftOperandValue, Results,
      [this](llvm::SmallVectorImpl<IeleRValue *> &Results){ Results.push_back(IeleRValue::Create(iele::IeleIntConstant::getOne(&Context))); },
      [&RightOperand, this](llvm::SmallVectorImpl<IeleRValue *> &Results){Results.push_back(compileExpression(RightOperand));});
    solAssert(Results.size() == 1, "Boolean operators have a single results");
    return Results[0]->read(CompilingBlock)->getValue();
  } else {
    appendConditional(
      LeftOperandValue, Results,
      [&RightOperand, this](llvm::SmallVectorImpl<IeleRValue *> &Results){Results.push_back(compileExpression(RightOperand));},
      [this](llvm::SmallVectorImpl<IeleRValue *> &Results){Results.push_back(IeleRValue::Create(iele::IeleIntConstant::getZero(&Context))); });
    solAssert(Results.size() == 1, "Boolean operators have a single results");
    return Results[0]->read(CompilingBlock)->getValue();
  }
}

iele::IeleLocalVariable *IeleCompiler::appendBinaryOperator(
    Token Opcode,
    iele::IeleValue *LeftOperand,
    iele::IeleValue *RightOperand,
    TypePointer ResultType) {
  // Find corresponding IELE binary opcode.
  iele::IeleInstruction::IeleOps BinOpcode;

  bool unchecked = getArithmetic() == Arithmetic::Wrapping;

  bool fixed = false, issigned = false;
  int nbytes = 0;
  switch (ResultType->category()) {
  case Type::Category::Integer: {
    const IntegerType *type = dynamic_cast<const IntegerType *>(ResultType);
    fixed = !type->isUnbound();
    nbytes = fixed ? type->numBits() / 8 : 0;
    issigned = type->isSigned();
    break;
  }
  case Type::Category::FixedBytes: {
    const FixedBytesType *type = dynamic_cast<const FixedBytesType *>(ResultType);
    fixed = true;
    nbytes = type->numBytes();
    break;
  }
  default: break;
  }

  iele::IeleValue *OriginalRightOperand = RightOperand;

  switch (Opcode) {
  case Token::Add:                BinOpcode = iele::IeleInstruction::Add; break;
  case Token::Sub: {
    // In case of unsigned unbounded subtraction, we check for negative result
    // and throw if that is the case.
    if (!fixed && !issigned) {
      iele::IeleLocalVariable *NegativeResult =
        iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
      iele::IeleInstruction::CreateBinOp(
          iele::IeleInstruction::CmpLt, NegativeResult, LeftOperand,
          RightOperand, CompilingBlock);
      appendRevert(NegativeResult);
    }
    BinOpcode = iele::IeleInstruction::Sub; break;
  }
  case Token::Mul: {
    if (fixed && unchecked) {
      // In case of fixed-width multiplication, create the instruction as a
      // mulmod.
      iele::IeleValue *ModulusValue =
        iele::IeleIntConstant::Create(&Context, bigint(1) << (nbytes * 8));
      iele::IeleLocalVariable *Result =
        iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
      iele::IeleInstruction::CreateTernOp(
        iele::IeleInstruction::MulMod, Result, LeftOperand, RightOperand,
        ModulusValue, CompilingBlock);

      // Sign extend if necessary.
      if (issigned) {
        iele::IeleValue *NBytesValue =
          iele::IeleIntConstant::Create(&Context, bigint(nbytes));
        iele::IeleInstruction::CreateBinOp(
          iele::IeleInstruction::SExt, Result, NBytesValue, Result,
          CompilingBlock);
      }
      return Result;
    }
    // Else, create normal multiplication.
    BinOpcode = iele::IeleInstruction::Mul; break;
  }
  case Token::Div:                BinOpcode = iele::IeleInstruction::Div; break;
  case Token::Mod:                BinOpcode = iele::IeleInstruction::Mod; break;
  case Token::Exp: {
    if (fixed && unchecked) {
      // In case of fixed-width exponentiation, create the instruction as an
      // expmod.
      iele::IeleValue *ModulusValue =
        iele::IeleIntConstant::Create(&Context, bigint(1) << (nbytes * 8));
      iele::IeleLocalVariable *Result =
        iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
      iele::IeleInstruction::CreateTernOp(
        iele::IeleInstruction::ExpMod, Result, LeftOperand, RightOperand,
        ModulusValue, CompilingBlock);

      // Sign extend if necessary.
      if (issigned) {
        iele::IeleValue *NBytesValue =
          iele::IeleIntConstant::Create(&Context, bigint(nbytes));
        iele::IeleInstruction::CreateBinOp(
          iele::IeleInstruction::SExt, Result, NBytesValue, Result,
          CompilingBlock);
      }
      return Result;
    }
    // Else, create normal exponentiation.
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
  }
  BOOST_FALLTHROUGH; // Darwin
  // Fedora (NB: this NEEDS to be immediately before "case")
  // fall through    
  case Token::SHL: {
    BinOpcode = iele::IeleInstruction::Shift; 
    bigint min = 0;
    appendRangeCheck(OriginalRightOperand, &min, nullptr);
    break;
  }
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
    case Token::Mod: {
      if (unchecked)
        appendMask(Result, Result, nbytes, issigned);
      else
        appendRangeCheck(IeleRValue::Create(Result), *ResultType);
      break;
    }
    case Token::SHL: {
      appendMask(Result, Result, nbytes, issigned);
      break;
    }
    case Token::Mul:
    case Token::Exp: {
      solAssert(!unchecked, "");
      appendRangeCheck(IeleRValue::Create(Result), *ResultType);
      break;
    }
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

 void IeleCompiler::appendTypeConversions(
  llvm::SmallVectorImpl<IeleRValue *> &Results, 
  llvm::SmallVectorImpl<IeleRValue *> &RHSValues,
  TypePointer SourceType, TypePointers TargetTypes) {

  TypePointers RHSTypes;
  if (const TupleType *tupleType =
        dynamic_cast<const TupleType *>(SourceType)) {
    RHSTypes = tupleType->components();
  } else {
    RHSTypes = TypePointers{SourceType};
  }

  solAssert(RHSValues.size() == RHSTypes.size(),
            "IeleCompiler: Missing value in tuple in conditional expression");

  solAssert(TargetTypes.size() == RHSTypes.size(),
            "IeleCompiler: Missing value in tuple in conditional expression");

  int i = 0;
  for (IeleRValue *RHSValue : RHSValues) {
    TypePointer LHSType = TargetTypes[i];
    TypePointer RHSType = RHSTypes[i];
    IeleRValue *Result = appendTypeConversion(RHSValue, RHSType, LHSType);
    Results.push_back(Result);
    i++;
  }
}


IeleRValue *IeleCompiler::appendTypeConversion(IeleRValue *Value, TypePointer SourceType, TypePointer TargetType) {
  if (*SourceType == *TargetType) {
    return Value;
  }
  iele::IeleLocalVariable *convertedValue =
    iele::IeleLocalVariable::Create(&Context, "type.converted", CompilingFunction);
  IeleRValue *Result = IeleRValue::Create(convertedValue);
  switch (SourceType->category()) {
  case Type::Category::FixedBytes: {
    const FixedBytesType &srcType = dynamic_cast<const FixedBytesType &>(*SourceType);
    if (TargetType->category() == Type::Category::Integer) {
      IntegerType const& targetType = dynamic_cast<const IntegerType &>(*TargetType);
      if (!targetType.isUnbound() && targetType.numBits() < srcType.numBytes() * 8) {
        appendMask(convertedValue, Value->getValue(), targetType.numBits() / 8, targetType.isSigned());
        return Result;
      }
      return Value;
    } else if (TargetType->category() == Type::Category::Address) {
      if (160 < srcType.numBytes() * 8) {
        appendMask(convertedValue, Value->getValue(), 160 / 8, false);
        return Result;
      }
      return Value;
    } else {
      solAssert(TargetType->category() == Type::Category::FixedBytes, "Invalid type conversion requested.");
      const FixedBytesType &targetType = dynamic_cast<const FixedBytesType &>(*TargetType);
      appendShiftBy(convertedValue, Value->getValue(), 8 * (targetType.numBytes() - srcType.numBytes()));
      return Result;
    }
  }
  case Type::Category::Enum:
    solAssert(*SourceType == *TargetType || TargetType->category() == Type::Category::Integer, "Invalid enum conversion");
    return Value;
  case Type::Category::FixedPoint:
    solAssert(false, "not implemented yet");
  case Type::Category::Function: {
    solAssert(TargetType->category() == Type::Category::Address ||
              TargetType->category() == Type::Category::Function,
              "Invalid type conversion requested.");
    const FunctionType &sourceType = dynamic_cast<const FunctionType &>(*SourceType);
    if (TargetType->category() == Type::Category::Address) {
      solAssert(sourceType.kind() == FunctionType::Kind::External, "Only external function type can be converted to address.");
      solAssert(Value->getValues().size() == 2, "Incorrect number of rvalues.");
    } else { // Type::Category::Function
      solAssert(Value->getValues().size() == 1 || Value->getValues().size() == 2,
                "Incorrect number of rvalues.");
    }
    return IeleRValue::Create(Value->getValues()[0]);
  }
  case Type::Category::Struct:
  case Type::Category::Array:
    if (shouldCopyStorageToMemory(*TargetType, *SourceType))
      return IeleRValue::Create(appendCopyFromStorageToMemory(TargetType, Value, SourceType));
    return Value;
  case Type::Category::Integer:
  case Type::Category::Address:
  case Type::Category::RationalNumber:
  case Type::Category::Contract: {
    switch(TargetType->category()) {
    case Type::Category::FixedBytes: {
      solAssert(SourceType->category() == Type::Category::Integer ||
                SourceType->category() == Type::Category::Address ||
                SourceType->category() == Type::Category::RationalNumber,
        "Invalid conversion to FixedBytesType requested.");
      if (SourceType->category() == Type::Category::Integer) {
        IntegerType const& sourceType = dynamic_cast<const IntegerType &>(*SourceType);
        const FixedBytesType &targetType = dynamic_cast<const FixedBytesType &>(*TargetType);
        if (sourceType.isUnbound() || targetType.numBytes() * 8 < sourceType.numBits()) {
          appendMask(convertedValue, Value->getValue(), targetType.numBytes(), false);
          return Result;
        }
      } else if (SourceType->category() == Type::Category::Address) {
        const FixedBytesType &targetType = dynamic_cast<const FixedBytesType &>(*TargetType);
        if (targetType.numBytes() * 8 < 160) {
          appendMask(convertedValue, Value->getValue(), targetType.numBytes(), false);
          return Result;
        }
      }
      return Value;
    }
    case Type::Category::Enum: {
      appendRangeCheck(Value, *TargetType);
      return Value;
    }
    case Type::Category::StringLiteral:
      solAssert(false, "not implemented yet");
      break;
    case Type::Category::FixedPoint:
      solUnimplemented("Not yet implemented - FixedPointType.");
      break;
    case Type::Category::Integer:
    case Type::Category::Address:
    case Type::Category::Contract: {
      IntegerType const& targetType = TargetType->category() == Type::Category::Integer
        ? dynamic_cast<const IntegerType &>(*TargetType) : *TypeProvider::uint(160);
      if (SourceType->category() == Type::Category::RationalNumber) {
        solAssert(!dynamic_cast<const RationalNumberType &>(*SourceType).isFractional(), "not implemented yet");
      }
      if (targetType.isUnbound()) {
        if (!targetType.isSigned()) {
          if (iele::IeleIntConstant *constant = llvm::dyn_cast<iele::IeleIntConstant>(Value->getValue())) {
            if (constant->getValue() < 0) {
              appendRevert();
            }
          } else if (auto srcType = dynamic_cast<const IntegerType *>(&*SourceType)) {
            if (srcType->isSigned()) {
              bigint min = 0;
              appendRangeCheck(Value->getValue(), &min, nullptr);
            }
          }
        }
        return Value;
      }
      if (iele::IeleIntConstant *constant = llvm::dyn_cast<iele::IeleIntConstant>(Value->getValue())) {
        if (constant->getValue() < (targetType.isSigned() ? bigint(1) << targetType.numBits() - 1 : bigint(1) << targetType.numBits())
          && constant->getValue() >= (targetType.isSigned() ? -(bigint(1) << targetType.numBits() - 1) : bigint(0))) {
          return Value;
        }
      }
      appendMask(convertedValue, Value->getValue(), targetType.numBits() / 8, targetType.isSigned());
      return Result;
    }
    default:
      solAssert(false, "Invalid type conversion requested.");
      return nullptr;
    }
  }
  /* Falls through. */
  case Type::Category::Bool: {
    solAssert(*SourceType == *TargetType, "Invalid conversion for bool.");
    return Value;
  }
  case Type::Category::StringLiteral: {
    const auto &literalType = dynamic_cast<const StringLiteralType &>(*SourceType);
    std::string value = literalType.value();
    switch(TargetType->category()) {
    case Type::Category::FixedBytes: {
      const FixedBytesType &targetType = dynamic_cast<const FixedBytesType &>(*TargetType);
      unsigned len = targetType.numBytes();
      if (value.size() < len)
        len = value.size();
      std::string choppedValue = value.substr(0, len);
      char paddedValue[32] = {0};
      memcpy(paddedValue, choppedValue.c_str(), len);
      std::string paddedValueStr = std::string(paddedValue, targetType.numBytes());
      bigint value = bigint(u256(h256(bytesConstRef(paddedValueStr), h256::AlignRight)));
      iele::IeleIntConstant *Result = iele::IeleIntConstant::Create(
        &Context, value, true);
      return IeleRValue::Create(Result);
    }
    case Type::Category::Array: {
      const ArrayType &targetType = dynamic_cast<const ArrayType &>(*TargetType);
      solAssert(targetType.isByteArray(), "cannot convert string literal to non-byte-array");
      iele::IeleValue *Ptr = appendArrayAllocation(targetType);
      iele::IeleIntConstant *Length = iele::IeleIntConstant::Create(
        &Context, value.size());
      iele::IeleInstruction::CreateStore(
        Length, Ptr, CompilingBlock);
      iele::IeleLocalVariable *ValueAddress =
        iele::IeleLocalVariable::Create(&Context, "string.value.address", CompilingFunction);
      iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Add, ValueAddress, Ptr,
        iele::IeleIntConstant::getOne(&Context),
        CompilingBlock);
      iele::IeleInstruction::CreateStore(
        Value->getValue(), ValueAddress, CompilingBlock);
      return IeleRValue::Create(Ptr);
    }
    default:
      solAssert(false, "not implemented yet");
    }
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
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::SExt, Result, NBytesValue, Result,
      CompilingBlock);
  }
}

iele::IeleLocalVariable *IeleCompiler::appendMemorySpill() {
  // Find last used memory location by loading the memory address zero.
  iele::IeleLocalVariable *LastUsed =
      iele::IeleLocalVariable::Create(&Context, "last.used", CompilingFunction);
  iele::IeleInstruction::CreateLoad(
      LastUsed, iele::IeleIntConstant::getOne(&Context), CompilingBlock);
  // Get next free memory location by increasing last used by one.
  iele::IeleLocalVariable *NextFree =
      iele::IeleLocalVariable::Create(&Context, "next.free", CompilingFunction);
  iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Add, NextFree, LastUsed,
      iele::IeleIntConstant::getOne(&Context), CompilingBlock);
  iele::IeleInstruction::CreateStore(
      NextFree, iele::IeleIntConstant::getOne(&Context), CompilingBlock);
  iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Add, NextFree, NextFree,
      iele::IeleIntConstant::getOne(&Context), CompilingBlock);

  return NextFree;
}

bool IeleCompiler::shouldCopyStorageToStorage(const Type &ToType, const IeleLValue *To,
                                              const Type &From) const {
  return dynamic_cast<const ReadOnlyLValue *>(To) &&
         From.dataStoredIn(DataLocation::Storage) &&
         ToType.dataStoredIn(DataLocation::Storage);
}

bool IeleCompiler::shouldCopyMemoryToStorage(const Type &ToType, const IeleLValue *To,
                                             const Type &FromType) const {
  return dynamic_cast<const ReadOnlyLValue *>(To) &&
         (FromType.dataStoredIn(DataLocation::Memory) ||
          FromType.dataStoredIn(DataLocation::CallData)) &&
         ToType.dataStoredIn(DataLocation::Storage);
}

bool IeleCompiler::shouldCopyMemoryToMemory(const Type &ToType, const IeleLValue *To,
                                             const Type &FromType) const {
  return ((dynamic_cast<const ReadOnlyLValue *>(To) &&
             FromType.dataStoredIn(DataLocation::Memory)) ||
          FromType.dataStoredIn(DataLocation::CallData)) &&
         (ToType.dataStoredIn(DataLocation::Memory) ||
          ToType.dataStoredIn(DataLocation::CallData));
}

bool IeleCompiler::shouldCopyStorageToMemory(const Type &ToType,
                                             const Type &FromType) const {
  return (ToType.dataStoredIn(DataLocation::Memory) ||
          ToType.dataStoredIn(DataLocation::CallData)) &&
         FromType.dataStoredIn(DataLocation::Storage);
}

void IeleCompiler::appendIeleRuntimeFill(
     iele::IeleValue *To, iele::IeleValue *NumSlots, iele::IeleValue *Value,
     DataLocation Loc) {
  std::string name;
  switch(Loc) {
  case DataLocation::Storage:
    name = "ielert.storage.fill";
    break;
  case DataLocation::CallData:
  case DataLocation::Memory:
    name = "ielert.memory.fill";
    break;
  }
  CompilingContract->setIncludeMemoryRuntime(true);
  iele::IeleGlobalVariable *Callee =
    iele::IeleGlobalVariable::Create(&Context, name);
  llvm::SmallVector<iele::IeleLocalVariable *, 0> EmptyResults;
  llvm::SmallVector<iele::IeleValue *, 3> Arguments;
  Arguments.push_back(To);
  Arguments.push_back(NumSlots);
  Arguments.push_back(Value);
  iele::IeleInstruction::CreateInternalCall(EmptyResults, Callee, Arguments,
                                            CompilingBlock);
}

iele::IeleLocalVariable *IeleCompiler::appendIeleRuntimeAllocateMemory(
     iele::IeleValue *NumSlots) {
  CompilingContract->setIncludeMemoryRuntime(true);
  iele::IeleLocalVariable *Result =
    iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
  iele::IeleGlobalVariable *Callee =
    iele::IeleGlobalVariable::Create(&Context, "ielert.memory.allocate");
  llvm::SmallVector<iele::IeleLocalVariable *, 1> Results(1, Result);
  llvm::SmallVector<iele::IeleValue *, 1> Arguments(1, NumSlots);
  iele::IeleInstruction::CreateInternalCall(Results, Callee, Arguments,
                                            CompilingBlock);
  return Result;
}

iele::IeleLocalVariable *IeleCompiler::appendIeleRuntimeAllocateStorage(
     iele::IeleValue *NumSlots) {
  CompilingContract->setIncludeStorageRuntime(true);
  iele::IeleLocalVariable *Result =
    iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
  iele::IeleGlobalVariable *Callee =
    iele::IeleGlobalVariable::Create(&Context, "ielert.storage.allocate");
  llvm::SmallVector<iele::IeleLocalVariable *, 1> Results(1, Result);
  llvm::SmallVector<iele::IeleValue *, 1> Arguments(1, NumSlots);
  iele::IeleInstruction::CreateInternalCall(Results, Callee, Arguments,
                                            CompilingBlock);
  return Result;
}

void IeleCompiler::appendIeleRuntimeCopy(
    iele::IeleValue *From, iele::IeleValue *To, iele::IeleValue *NumSlots,
    DataLocation FromLoc, DataLocation ToLoc) {
  CompilingContract->setIncludeMemoryRuntime(true);
  std::string name;
  if (FromLoc == DataLocation::Storage && ToLoc == DataLocation::Storage) {
    name = "ielert.storage.copy.to.storage";
  } else if (FromLoc == DataLocation::Storage && ToLoc == DataLocation::Memory) {
    name = "ielert.storage.copy.to.memory";
  } else if (FromLoc == DataLocation::Memory && ToLoc == DataLocation::Memory) {
    name = "ielert.memory.copy.to.memory";
  } else if (FromLoc == DataLocation::Memory && ToLoc == DataLocation::Storage) {
    name = "ielert.memory.copy.to.storage";
  } else {
    solAssert(false, "Not implemented in IELE");
  }
  
  iele::IeleGlobalVariable *Callee =
    iele::IeleGlobalVariable::Create(&Context, name);
  llvm::SmallVector<iele::IeleLocalVariable *, 0> EmptyResults;
  llvm::SmallVector<iele::IeleValue *, 3> Arguments;
  Arguments.push_back(From);
  Arguments.push_back(To);
  Arguments.push_back(NumSlots);
  iele::IeleInstruction::CreateInternalCall(EmptyResults, Callee, Arguments,
                                            CompilingBlock);
}
