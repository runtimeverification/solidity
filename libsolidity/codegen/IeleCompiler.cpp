#include "libsolidity/codegen/IeleCompiler.h"
#include "libsolidity/codegen/IeleLValue.h"

#include <libsolidity/interface/Exceptions.h>

#include <libdevcore/SHA3.h>

#include "libiele/IeleContract.h"
#include "libiele/IeleGlobalVariable.h"
#include "libiele/IeleIntConstant.h"

#include <iostream>
#include "llvm/Support/raw_ostream.h"

using namespace dev;
using namespace dev::solidity;

auto UInt = std::make_shared<IntegerType>();
auto SInt = std::make_shared<IntegerType>(IntegerType::Modifier::Signed);
auto Address = std::make_shared<IntegerType>(160, IntegerType::Modifier::Address);

iele::IeleValue *IeleCompiler::Value::rval(iele::IeleBlock *Block) {
  return isLValue ? LValue->read(Block) : RValue;
}

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
    IeleFunctionName = function.name() + "." + function.type()->identifier();

  if (isMostDerived(&function)) {
    return IeleFunctionName;
  } else {
    return getIeleNameForContract(contractFor(&function)) + "." + IeleFunctionName;
  }
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
      if (d->name() == decl->name() && !decl->isConstructor() && FunctionType(*decl).hasEqualArgumentTypes(FunctionType(*d))) {
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
      if (function.name() == decl->name() && !decl->isConstructor() && FunctionType(*decl).hasEqualArgumentTypes(FunctionType(function))) {
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

bool hasTwoFunctions(const FunctionType &function, bool isConstructor, bool isLibrary) {
  if (isConstructor || isLibrary) {
    return false;
  }
  if (function.declaration().visibility() != Declaration::Visibility::Public) {
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

iele::IeleGlobalValue *IeleCompiler::convertFunctionToInternal(iele::IeleGlobalValue *Callee) {
  if (iele::IeleFunction *function = llvm::dyn_cast<iele::IeleFunction>(Callee)) {
    iele::IeleValueSymbolTable *ST = CompilingContract->getIeleValueSymbolTable();
    solAssert(ST,
              "IeleCompiler: failed to access compiling contract's symbol table.");
    std::string name = function->getName();
    return llvm::dyn_cast<iele::IeleGlobalValue>(ST->lookup(name + ".internal"));
  }
  solAssert(false, "not implemented yet: function pointers");
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

void IeleCompiler::compileContract(
    const ContractDefinition &contract,
    const std::map<const ContractDefinition *, iele::IeleContract *> &contracts) {


  CompiledContracts = contracts;

  // Create IeleContract.
  CompilingContract = iele::IeleContract::Create(&Context, getIeleNameForContract(&contract));

  // Add IELE global variables and functions to contract's symbol table by
  // iterating over state variables and functions of this contract and its base
  // contracts.
  std::vector<ContractDefinition const*> bases =
    contract.annotation().linearizedBaseContracts;
  // Store the current contract.
  CompilingContractInheritanceHierarchy = bases;
  bool most_derived = true;
  
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
        // This is the actual constructo, add it to the symbol table.
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
      if (function->isConstructor() || function->isFallback() ||
          !function->isImplemented())
        continue;
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


  // Finally add the fallback function to the contract's symbol table.
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

  // Visit base constructors. If any don't exist create an
  // @<base-contract-name>init function that contains only state variable
  // initialization.
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
            if (!paramName.empty()) {
              iele::IeleArgument::Create(&Context, paramName, CompilingFunction);
              VarNameMap[0][paramName] = paramName;
            }
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
            if (!paramName.empty()) {
              iele::IeleArgument::Create(&Context, paramName, CompilingFunction);
              VarNameMap[0][paramName] = paramName;
          }
        }
      }
    }
    
    appendDefaultConstructor(CompilingContractASTNode);
    iele::IeleInstruction::CreateRetVoid(CompilingBlock);
    appendRevertBlocks();
    CompilingBlock = nullptr;
    CompilingFunction = nullptr;
  }

  for (auto dep : dependencies) {
    if (dep->isLibrary()) {
      for (const FunctionDefinition *function : dep->definedFunctions()) {
        if (function->isConstructor() || function->isFallback() || !function->isImplemented())
          continue;
        function->accept(*this);
      }
    }
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
    iele::IeleValue *LoadedValue = compileExpression(*stateVariable->value());
    TypePointer rhsType = stateVariable->value()->annotation().type;
    LoadedValue = appendTypeConversion(LoadedValue, rhsType, stateVariable->annotation().type);
    if (auto arrayType = dynamic_cast<const ArrayType *>(stateVariable->annotation().type.get())) {
      if (arrayType->isByteArray()) {
        LoadedValue = encoding(LoadedValue, stateVariable->annotation().type);
      } else {
        solAssert(false, "constant non-byte array");
      }
    }
    Returns.push_back(LoadedValue);
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

    iele::IeleValue *Address = makeLValue(GV, returnType, DataLocation::Storage)->read(CompilingBlock);

    for (unsigned i = 0; i < paramTypes.size(); i++) {
      if (auto mappingType = dynamic_cast<const MappingType *>(returnType.get())) {
        Address = appendMappingAccess(*mappingType, parameters[i], Address)->read(CompilingBlock);
        returnType = mappingType->valueType();
      } else if (auto arrayType = dynamic_cast<const ArrayType *>(returnType.get())) {
        Address = appendArrayAccess(*arrayType, parameters[i], Address, DataLocation::Storage)->read(CompilingBlock);
        returnType = arrayType->baseType();
      } else {
        solAssert(false, "Index access is allowed only for \"mapping\" and \"array\" types.");
      }
    }
    auto returnTypes = accessorType.returnParameterTypes();
    solAssert(returnTypes.size() >= 1, "accessor must return a type");
    if (auto structType = dynamic_cast<const StructType *>(returnType.get())) {
      auto const& names = accessorType.returnParameterNames();
      for (unsigned i = 0; i < names.size(); i++) {
        TypePointer memberType = structType->memberType(names[i]);
        iele::IeleValue *Value = appendStructAccess(*structType, Address, names[i], DataLocation::Storage)->read(CompilingBlock);
        Returns.push_back(encoding(Value, memberType));
      }
    } else {
      Returns.push_back(encoding(Address, returnType));
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
    llvm::SmallVector<iele::IeleArgument *, 4> parameters;
    llvm::SmallVector<iele::IeleValue *, 4> paramValues;
    // Visit formal arguments.
    for (const ASTPointer<const VariableDeclaration> &arg : function.parameters()) {
      std::string genName = arg->name() + getNextVarSuffix();
      auto param = iele::IeleArgument::Create(&Context, genName, CompilingFunction);
      parameters.push_back(param);
      paramValues.push_back(param);
    }
    llvm::SmallVector<iele::IeleLocalVariable *, 4> ReturnParameters;
    llvm::SmallVector<iele::IeleValue *, 4> ReturnParameterValues;
 
    // Visit formal return parameters.
    for (const ASTPointer<const VariableDeclaration> &ret : function.returnParameters()) {
      std::string genName = ret->name() + getNextVarSuffix();
      auto param = iele::IeleLocalVariable::Create(&Context, genName, CompilingFunction);
      ReturnParameters.push_back(param);
    }
  
    // Create the entry block.
    CompilingBlock =
      iele::IeleBlock::Create(&Context, "entry", CompilingFunction);
 
    if (!function.isPayable()) {
      appendPayableCheck();
    }

    for (unsigned i = 0; i < function.parameters().size(); i++) {
      const auto &arg = *function.parameters()[i];
      iele::IeleArgument *ieleArg = parameters[i];
      const auto &argType = arg.type();
      if (argType->isValueType()) {
        appendRangeCheck(ieleArg, *argType);
      } else {
        iele::IeleInstruction::CreateAssign(
          ieleArg, decoding(ieleArg, argType),
          CompilingBlock);
      }
    }
    iele::IeleFunction *Callee = llvm::dyn_cast<iele::IeleFunction>(ST->lookup(name));
    iele::IeleInstruction::CreateInternalCall(
      ReturnParameters, Callee, paramValues, CompilingBlock);

    for (unsigned i = 0; i < function.returnParameters().size(); i++) {
      ReturnParameterValues.push_back(encoding(ReturnParameters[i], function.returnParameters()[i]->type()));
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
  std::vector<iele::IeleArgument *> parameters;
  // returnParameters.clear(); // otherwise, stuff keep getting added, regardless
  //                           // of which function we are in (i.e. it breaks)

  // Visit formal arguments.
  for (const ASTPointer<const VariableDeclaration> &arg : function.parameters()) {
    std::string genName = arg->name() + getNextVarSuffix();
    parameters.push_back(iele::IeleArgument::Create(&Context, genName, CompilingFunction));
    // No need to keep track of the mapping for omitted args, since they will
    // never be referenced.
    if (!(arg->name() == ""))
       VarNameMap[NumOfModifiers][arg->name()] = genName;
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
            iele::IeleArgument::Create(&Context, paramName, CompilingFunction);
            VarNameMap[NumOfModifiers][paramName] = paramName;
          }
        }
      }
    }
  }

  // We store the return params names, which we'll use when generating a default
  // `ret`.
  llvm::SmallVector<std::string, 4> ReturnParameterNames;
  llvm::SmallVector<TypePointer, 4> ReturnParameterTypes;

  // Visit formal return parameters.
  for (const ASTPointer<const VariableDeclaration> &ret : function.returnParameters()) {
    std::string genName = ret->name() + getNextVarSuffix();
    ReturnParameterNames.push_back(genName);
    ReturnParameterTypes.push_back(ret->type());
    CompilingFunctionReturnParameters.push_back(iele::IeleLocalVariable::Create(&Context, genName, CompilingFunction));
    // No need to keep track of the mapping for omitted return params, since
    // they will never be referenced.
    if (!(ret->name() == ""))
      VarNameMap[NumOfModifiers][ret->name()] = genName; 
  }

  // Visit local variables.
  for (const VariableDeclaration *local: function.localVariables()) {
    std::string genName = local->name() + getNextVarSuffix();
    VarNameMap[NumOfModifiers][local->name()] = genName; 
    iele::IeleLocalVariable::Create(&Context, genName, CompilingFunction);
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

  if (function.isPublic() && !hasTwoFunctions(FunctionType(function), function.isConstructor(), false) && !contractFor(&function)->isLibrary()) {
    if (!function.isPayable()) {
      appendPayableCheck();
    }
    for (unsigned i = 0; i < function.parameters().size(); i++) {
      const auto &arg = *function.parameters()[i];
      iele::IeleArgument *ieleArg = parameters[i];
      const auto &argType = arg.type();
      if (argType->isValueType()) {
        appendRangeCheck(ieleArg, *argType);
      } else if (!function.isConstructor() || isMostDerived(&function)) {
        iele::IeleInstruction::CreateAssign(
          ieleArg, decoding(ieleArg, argType),
          CompilingBlock);
      }
    }
  }
  // If the function is a constructor, visit state variables and add
  // initialization code.
  if (function.isConstructor())
    appendDefaultConstructor(CompilingContractASTNode);

  // Initialize local variables and return params.
  iele::IeleValueSymbolTable *FunST =
    CompilingFunction->getIeleValueSymbolTable();
  solAssert(FunST,
            "IeleCompiler: failed to access compiling function's symbol "
            "table.");
  for (const VariableDeclaration *local: function.localVariables()) {
    iele::IeleValue *Local =
      FunST->lookup(VarNameMap[NumOfModifiers][local->name()]);
    solAssert(Local, "IeleCompiler: missing local variable");
    appendLocalVariableInitialization(
      llvm::cast<iele::IeleLocalVariable>(Local), local);
  }
  for (unsigned i = 0; i < function.returnParameters().size(); i++) {
    const ASTPointer<const VariableDeclaration> &ret = function.returnParameters()[i];
    std::string name = ReturnParameterNames[i];
    iele::IeleValue *RetParam =
      FunST->lookup(name);
    solAssert(RetParam, "IeleCompiler: missing return parameter");
    appendLocalVariableInitialization(
      llvm::cast<iele::IeleLocalVariable>(RetParam), &*ret);
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
  appendReturn(function, ReturnParameterNames, ReturnParameterTypes);

  // Append the exception blocks if needed.
  appendRevertBlocks();

  CompilingBlock = nullptr;
  CompilingFunction = nullptr;
  CompilingFunctionReturnParameters.clear(); // otherwise, stuff keep getting added, regardless
                            // of which function we are in (i.e. it breaks)
  return false;
}

void IeleCompiler::appendReturn(const FunctionDefinition &function, 
    llvm::SmallVector<std::string, 4> ReturnParameterNames,
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

      for (unsigned i = 0; i < ReturnParameterNames.size(); i++) {
          const std::string paramName = ReturnParameterNames[i];
          TypePointer paramType = ReturnParameterTypes[i];
          iele::IeleValue *param = ST->lookup(paramName);
          solAssert(param, "IeleCompiler: couldn't find parameter name in symbol table.");
          if (function.isPublic() && !hasTwoFunctions(FunctionType(function), function.isConstructor(), false) && !contractFor(&function)->isLibrary()) {
            Returns.push_back(encoding(param, paramType));
          } else {
            Returns.push_back(param);
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

  solAssert(!ReturnBlocks.empty(), "IeleCompiler: return jmp destination not set");
  
  if (!returnExpr) {
    connectWithUnconditionalJump(CompilingBlock, ReturnBlocks.top());    
    return false;
  }

  // Visit return expression.
  llvm::SmallVector<iele::IeleValue*, 4> Values;
  compileTuple(*returnExpr, Values);

  llvm::SmallVector<iele::IeleValue *, 4> ReturnValues;

  TypePointers returnTypes = FunctionType(*CompilingFunctionASTNode).returnParameterTypes();
  
  appendTypeConversions(ReturnValues, Values, returnExpr->annotation().type, returnTypes);

  llvm::SmallVector<iele::IeleValue *, 4> EncodedReturnValues;
  for (unsigned i = 0; i < ReturnValues.size(); i++) {
    iele::IeleValue *Value = ReturnValues[i];
    TypePointer type = returnTypes[i];
    if (CompilingFunctionASTNode->isPublic() && !hasTwoFunctions(FunctionType(*CompilingFunctionASTNode), CompilingFunctionASTNode->isConstructor(), false) && !contractFor(CompilingFunctionASTNode)->isLibrary()) {
      EncodedReturnValues.push_back(encoding(Value, type));
    } else {
      EncodedReturnValues.push_back(Value);
    }
  }

  for (unsigned i = 0; i < EncodedReturnValues.size(); ++i) {
    iele::IeleInstruction::CreateAssign(
      CompilingFunctionReturnParameters[i], EncodedReturnValues[i], CompilingBlock);        
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
    if (dynamic_cast<ContractDefinition const*>(modifierInvocation->name()->annotation().referencedDeclaration)) {
      appendModifierOrFunctionCode();
    }
    else {
      // Retrieve modifier definition from its name
      const ModifierDefinition *modifier;
      if (contractFor(CompilingFunctionASTNode)->isLibrary()) {
        for (ModifierDefinition const* mod: contractFor(CompilingFunctionASTNode)->functionModifiers())
          if (mod->name() == modifierInvocation->name()->name())
            modifier = mod;
      } else {
        modifier = functionModifier(modifierInvocation->name()->name());
      }
      solAssert(modifier, "Could not find modifier");

      // Visit the modifier's parameters
      for (const ASTPointer<const VariableDeclaration> &arg : modifier->parameters()) {
        std::string genName = arg->name() + getNextVarSuffix();
        VarNameMap[ModifierDepth][arg->name()] = genName;
        iele::IeleLocalVariable::Create(&Context, genName, CompilingFunction);
      }

      // Visit and initialize the modifier's local variables
      for (const VariableDeclaration *local: modifier->localVariables()) {
        std::string genName = local->name() + getNextVarSuffix();
        VarNameMap[ModifierDepth][local->name()] = genName;
        iele::IeleLocalVariable *Local =
          iele::IeleLocalVariable::Create(&Context, genName, CompilingFunction);
        appendLocalVariableInitialization(Local, local);
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
        // Check if we need to do a storage to memory copy.
        TypePointer LHSType = varDecl->annotation().type;
        TypePointer RHSType = RHSTypes[i];
        iele::IeleValue *RHSValue = RHSValues[i];
        RHSValue = appendTypeConversion(RHSValue, RHSType, LHSType);

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
  llvm::SmallVector<iele::IeleValue*, 4> Values;
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
  iele::IeleValue *ConditionValue = compileExpression(condition.condition());
  solAssert(ConditionValue, "IeleCompiler: failed to compile conditional condition.");

  llvm::SmallVector<iele::IeleLocalVariable *, 4> Results;
  appendConditional(ConditionValue, Results,
    [&condition, this](llvm::SmallVectorImpl<iele::IeleValue *> &Results){
      appendConditionalBranch(Results, condition.trueExpression(), condition.annotation().type);
    },
    [&condition, this](llvm::SmallVectorImpl<iele::IeleValue *> &Results){
      appendConditionalBranch(Results, condition.falseExpression(), condition.annotation().type);
    });
  CompilingExpressionResult.insert(
    CompilingExpressionResult.end(), Results.begin(), Results.end());
  return false;
}

void IeleCompiler::appendConditionalBranch(
  llvm::SmallVectorImpl<iele::IeleValue *> &Results,
  const Expression &Expression,
  TypePointer Type) {

  llvm::SmallVector<iele::IeleValue *, 4> RHSValues;
  compileTuple(Expression, RHSValues);

  TypePointers LHSTypes;
  if (const TupleType *tupleType =
        dynamic_cast<const TupleType *>(Type.get())) {
    LHSTypes = tupleType->components();
  } else {
    LHSTypes = TypePointers{Type};
  }

  appendTypeConversions(Results, RHSValues, Expression.annotation().type, LHSTypes);
}

void IeleCompiler::appendConditional(
  iele::IeleValue *ConditionValue,
  llvm::SmallVectorImpl<iele::IeleLocalVariable *> &ResultValues,
  const std::function<void(llvm::SmallVectorImpl<iele::IeleValue *> &)> &TrueExpression,
  const std::function<void(llvm::SmallVectorImpl<iele::IeleValue *> &)> &FalseExpression) {
  // The condition target block is the if-true block.
  iele::IeleBlock *CondTargetBlock =
    iele::IeleBlock::Create(&Context, "if.true");

  // Connect the condition block with a conditional jump to the condition target
  // block.
  connectWithConditionalJump(ConditionValue, CompilingBlock, CondTargetBlock);

  // Append the expression for the if-false block and assign it to the result.
  llvm::SmallVector<iele::IeleValue *, 4> FalseValues;
  FalseExpression(FalseValues);

  for (iele::IeleValue *FalseValue : FalseValues) {
    iele::IeleLocalVariable *ResultValue =
      iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
    iele::IeleInstruction::CreateAssign(
      ResultValue, FalseValue, CompilingBlock);
    ResultValues.push_back(ResultValue);
  }

  iele::IeleBlock *IfTrueBlock = CondTargetBlock;

  // Since we have an if-false block, we need a new join block to jump to.
  iele::IeleBlock *JoinBlock = iele::IeleBlock::Create(&Context, "if.end");

  connectWithUnconditionalJump(CompilingBlock, JoinBlock);

  // Add the if-true block at the end of the function and generate its code.
  IfTrueBlock->insertInto(CompilingFunction);
  CompilingBlock = IfTrueBlock;

  // Append the expression for the if-true block and assign it to the result.
  llvm::SmallVector<iele::IeleValue *, 4> TrueValues;
  TrueExpression(TrueValues);
  solAssert(TrueValues.size() == FalseValues.size(), "mismatch in number of result values of conditional");

  for (unsigned i = 0; i < TrueValues.size(); i++) {
    iele::IeleValue *TrueValue = TrueValues[i];
    iele::IeleLocalVariable *ResultValue = ResultValues[i];
    iele::IeleInstruction::CreateAssign(
      ResultValue, TrueValue, CompilingBlock);
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

bool IeleCompiler::visit(const Assignment &assignment) {
  Token::Value op = assignment.assignmentOperator();
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
    RHSTypes.insert(RHSTypes.end(), RHSTupleType.components().begin(),
                    RHSTupleType.components().end());
    LHSTypes.insert(LHSTypes.end(), LHSTupleType.components().begin(),
                    LHSTupleType.components().end());
  } else {
    solAssert(LHSType->category() != Type::Category::Tuple,
              "IeleCompiler: found assignment from variable to tuple");
    RHSTypes.push_back(RHSType);
    LHSTypes.push_back(LHSType);
  }

  // Visit RHS.
  llvm::SmallVector<iele::IeleValue *, 4> RHSValues;
  compileTuple(RHS, RHSValues);

  // Save RHS elements that are named variables (locals and formal/return
  // aguments) into temporaries to ensure correct behavior of tuple assignment.
  // For example (a, b) = (b, a) should swap the contents of a and b, and hence
  // is not equivalent to either a = b; b = a or b = a; a = b but rather to
  // t1 = b; t2 = a; b = t2; a = t1.
  if (RHSType->category() == Type::Category::Tuple) {
    if (const TupleExpression *RHSTuple =
          dynamic_cast<const TupleExpression *>(&RHS)) {
      const auto &Components = RHSTuple->components();

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
            iele::IeleLocalVariable *TmpVar =
              iele::IeleLocalVariable::Create(
                  &Context, "tmp", CompilingFunction);
            iele::IeleInstruction::CreateAssign(
                TmpVar, RHSValues[i], CompilingBlock);
            RHSValues[i] = TmpVar;
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

    iele::IeleValue *RHSValue = RHSValues[i];
    TypePointer RHSComponentType = RHSTypes[i];
    solAssert(RHSValue, "IeleCompiler: Failed to compile RHS of assignement");

    RHSValue =
      appendTypeConversion(RHSValue, RHSComponentType, LHSComponentType);
    RHSValues[i] = RHSValue; // save the converted rhs for returning as part
                             // of the assignement expression result

    // Check if we need to do a memory/storage to storage copy. Only happens when
    // assigning to a state variable of refernece type.
    RHSComponentType = RHSComponentType->mobileType();
    if (shouldCopyStorageToStorage(LHSValue, *RHSComponentType))
      appendCopyFromStorageToStorage(LHSValue, LHSComponentType, RHSValue, RHSComponentType);
    else if (shouldCopyMemoryToStorage(*LHSComponentType, LHSValue, *RHSComponentType))
      appendCopyFromMemoryToStorage(LHSValue, LHSComponentType, RHSValue, RHSComponentType);
    else if (shouldCopyMemoryToMemory(*LHSComponentType, LHSValue, *RHSComponentType))
      appendCopyFromMemoryToMemory(LHSValue, LHSComponentType, RHSValue, RHSComponentType);
    else {
      // Check for compound assignment.
      if (op != Token::Assign) {
        Token::Value binOp = Token::AssignmentToBinaryOp(op);
        iele::IeleValue *LHSDeref = LHSValue->read(CompilingBlock);
        RHSValue = appendBinaryOperator(binOp, LHSDeref, RHSValue, LHSComponentType);
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
      iele::IeleValue *InitValue = compileExpression(*Components[i]);
      TypePointer componentType = Components[i]->annotation().type;
      InitValue = appendTypeConversion(InitValue, componentType, elementType);

      // Address in the new array in which we should copy the argument value.
      iele::IeleLocalVariable *AddressValue =
        iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
      iele::IeleInstruction::CreateBinOp(
          iele::IeleInstruction::Add, AddressValue, ArrayValue,
          iele::IeleIntConstant::Create(&Context, offset), CompilingBlock);

      // Do the copy.
      solAssert(!elementType->dataStoredIn(DataLocation::Storage),
                "IeleCompiler: found init value for inline array element in "
                "storage after type conversion");
      if (elementType->dataStoredIn(DataLocation::Memory))
        appendIeleRuntimeCopy(
            InitValue, AddressValue,
            iele::IeleIntConstant::Create(&Context, elementSize),
            DataLocation::Memory, DataLocation::Memory);
      else
        iele::IeleInstruction::CreateStore(InitValue, AddressValue,
                                           CompilingBlock);

      offset += elementSize;
    }

    // Return pointer to the newly allocated array.
    CompilingExpressionResult.push_back(ArrayValue);
    return false;
  }

  // Case of tuple.
  llvm::SmallVector<Value, 4> Results;
  for (unsigned i = 0; i < Components.size(); i++) {
    const ASTPointer<Expression> &component = Components[i];
    if (CompilingLValue && component) {
      Results.push_back(compileLValue(*component));
    } else if (CompilingLValue) {
      Results.push_back((IeleLValue *)nullptr);
    } else if (component) {
      Results.push_back(compileExpression(*component));
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
    CompilingExpressionResult.push_back(LiteralValue);
    return false;
  }

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
    iele::IeleIntConstant *Zero = iele::IeleIntConstant::getZero(&Context);
    iele::IeleLocalVariable *Result =
      appendBinaryOperator(Token::Sub, Zero, SubExprValue, ResultType);
    CompilingExpressionResult.push_back(Result);
    break;
  }
  case Token::Inc:    // ++ (pre- or postfix)
  case Token::Dec:  { // -- (pre- or postfix)
    Token::Value BinOperator =
      (UnOperator == Token::Inc) ? Token::Add : Token::Sub;
    // Compile subexpression as an lvalue.
    IeleLValue *SubExprValue =
      compileLValue(unaryOperation.subExpression());
    solAssert(SubExprValue, "IeleCompiler: Failed to compile operand.");
    // Get the current subexpression value, and save it in case of a postfix
    // oparation.
    iele::IeleValue *Before = SubExprValue->read(CompilingBlock);;
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
    SubExprValue->write(After, CompilingBlock);
    // Return result.
    CompilingExpressionResult.push_back(Result);
    break;
  }
  case Token::BitNot: { // ~
    // Visit subexpression.
    iele::IeleValue *SubExprValue =
      compileExpression(unaryOperation.subExpression());
    solAssert(SubExprValue, "IeleCompiler: Failed to compile operand.");

    bool fixed = false, issigned = false;
    int nbytes;
    switch (ResultType->category()) {
    case Type::Category::Integer: {
      const IntegerType *type = dynamic_cast<const IntegerType *>(ResultType.get());
      fixed = !type->isUnbound();
      nbytes = fixed ? type->numBits() / 8 : 0;
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

    CompilingExpressionResult.push_back(Result);
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

IeleLValue *IeleCompiler::makeLValue(iele::IeleValue *Address, TypePointer type, DataLocation Loc, iele::IeleValue *Offset) {
  if (Offset) {
    return ByteArrayLValue::Create(this, Address, Offset, Loc);
  }
  if (type->isValueType() || (type->isDynamicallySized() && Loc == DataLocation::Memory)) {
    if (auto arrayType = dynamic_cast<const ArrayType *>(type.get())) {
      if (arrayType->isByteArray()) {
        return ReadOnlyLValue::Create(Address);
      }
    }
    return AddressLValue::Create(this, Address, Loc);
  }
  return ReadOnlyLValue::Create(Address);
}

void IeleCompiler::appendLValueDelete(IeleLValue *LValue, TypePointer type) {
  if (type->isValueType()) {
    LValue->write(iele::IeleIntConstant::getZero(&Context), CompilingBlock);
    return;
  }
  iele::IeleValue *Address = LValue->read(CompilingBlock);
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
    for (const auto &member : structType.members(nullptr)) {
      // First compute the offset from the start of the struct.
      bigint offset;
      switch (structType.location()) {
      case DataLocation::Storage: {
        const auto& offsets = structType.storageOffsetsOfMember(member.name);
        offset = offsets.first;
        break;
      }
      case DataLocation::Memory: {
        offset = structType.memoryOffsetOfMember(member.name);
        break;
      case DataLocation::CallData: 
        solAssert(false, "Call data not supported in IELE");
      }
      }

      iele::IeleLocalVariable *Member =
        iele::IeleLocalVariable::Create(&Context, "fill.address", CompilingFunction);
      iele::IeleValue *OffsetValue =
        iele::IeleIntConstant::Create(&Context, offset);
      iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Add, Member, Address, OffsetValue,
        CompilingBlock);

      appendLValueDelete(makeLValue(Member, member.type, structType.location()), member.type);
    }
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

      // copy the size field
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

    iele::IeleBlock *LoopBodyBlock =
      iele::IeleBlock::Create(&Context, "delete.loop", CompilingFunction);
    CompilingBlock = LoopBodyBlock;

    iele::IeleBlock *LoopExitBlock =
      iele::IeleBlock::Create(&Context, "delete.loop.end");

    iele::IeleLocalVariable *DoneValue =
      iele::IeleLocalVariable::Create(&Context, "done", CompilingFunction);
    iele::IeleInstruction::CreateIsZero(
      DoneValue, SizeVariable, CompilingBlock);
    connectWithConditionalJump(DoneValue, CompilingBlock, LoopExitBlock);

    bigint elementSize;
    switch (arrayType.location()) {
    case DataLocation::Storage: {
      elementSize = elementType.storageSize();
      break;
    }
    case DataLocation::Memory: {
      elementSize = elementType.memorySize();
      break;
    }
    case DataLocation::CallData:
      solAssert(false, "not supported by IELE.");
    }

    appendLValueDelete(makeLValue(Element, arrayType.baseType(), arrayType.location()), arrayType.baseType());

    iele::IeleValue *ElementSizeValue =
        iele::IeleIntConstant::Create(&Context, elementSize);

    iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Add, Element, Element,
        ElementSizeValue,
        CompilingBlock);
    iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Sub, SizeVariable, SizeVariable,
        iele::IeleIntConstant::getOne(&Context),
        CompilingBlock);

    connectWithUnconditionalJump(CompilingBlock, LoopBodyBlock);
    LoopExitBlock->insertInto(CompilingFunction);
    CompilingBlock = LoopExitBlock;
    break;
  }
  case Type::Category::Mapping:
    // Delete on whole mappings is a noop in Solidity.
    break;
  default:
    solAssert(false, "not implemented yet");
  }
}

bool IeleCompiler::visit(const BinaryOperation &binaryOperation) {
  // Short-circuiting cases.
  if (binaryOperation.getOperator() == Token::Or || binaryOperation.getOperator() == Token::And) {
    iele::IeleValue *Result =
      appendBooleanOperator(binaryOperation.getOperator(),
                            binaryOperation.leftExpression(),
                            binaryOperation.rightExpression());
    CompilingExpressionResult.push_back(Result);
    return false;
  }

  const TypePointer &commonType = binaryOperation.annotation().commonType;

  // Check for compile-time constants.
  if (commonType->category() == Type::Category::RationalNumber) {
    iele::IeleIntConstant *LiteralValue =
      iele::IeleIntConstant::Create(
        &Context,
        commonType->literalValue(nullptr));
    CompilingExpressionResult.push_back(LiteralValue);
    return false;
  }

  // Visit operands.
  iele::IeleValue *LeftOperandValue = 
    compileExpression(binaryOperation.leftExpression());
  solAssert(LeftOperandValue, "IeleCompiler: Failed to compile left operand.");
  iele::IeleValue *RightOperandValue = 
    compileExpression(binaryOperation.rightExpression());
  solAssert(RightOperandValue, "IeleCompiler: Failed to compile right operand.");

  LeftOperandValue = appendTypeConversion(LeftOperandValue,
    binaryOperation.leftExpression().annotation().type,
    commonType);
  if (!Token::isShiftOp(binaryOperation.getOperator())) {
    RightOperandValue = appendTypeConversion(RightOperandValue,
      binaryOperation.rightExpression().annotation().type,
      commonType);
  }
  // Append the IELE code for the binary operator.
  iele::IeleValue *Result =
    appendBinaryOperator(binaryOperation.getOperator(),
                         LeftOperandValue, RightOperandValue,
                         binaryOperation.annotation().type);

  CompilingExpressionResult.push_back(Result);
  return false;
}

/// Perform encoding of the given values, and returns a register containing it
iele::IeleValue *IeleCompiler::encoding(
    iele::IeleValue *argument,
    TypePointer type,
    bool hash) {
  if (type->isValueType() || type->category() == Type::Category::Mapping) {
    return argument;
  }

  // Allocate cell 
  iele::IeleLocalVariable *NextFree = appendMemorySpill();

  llvm::SmallVector<iele::IeleValue *, 1> arguments;
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
  return EncodedVal;
}

void IeleCompiler::appendByteWidth(iele::IeleLocalVariable *Result, iele::IeleValue *Value) {
   // Calculate width in bytes of argument:
   // width_bytes(n) = ((log2(n) + 2) + 7) / 8
   //                = (log2(n) + 9) / 8
   //                = (log2(n) + 9) >> 3
  llvm::SmallVector<iele::IeleLocalVariable *, 1> Results;
   appendConditional(Value, Results,
    [&Value, this](llvm::SmallVectorImpl<iele::IeleValue *> &Results){
      iele::IeleLocalVariable *Result =
        iele::IeleLocalVariable::Create(&Context, "logarithm.base.2", CompilingFunction);
      iele::IeleInstruction::CreateLog2(Result, Value, CompilingBlock); 
      Results.push_back(Result);
    },
    [this](llvm::SmallVectorImpl<iele::IeleValue *> &Results){
      Results.push_back(iele::IeleIntConstant::getZero(&Context));
    });
   solAssert(Results.size() == 1, "Invalid number of conditional results in appendByteWidth");
   iele::IeleInstruction::CreateBinOp(
     iele::IeleInstruction::Add, Result, 
     Results[0],
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
    llvm::SmallVectorImpl<iele::IeleValue *> &arguments, 
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
  iele::IeleValue *ArgValue = LValue->read(CompilingBlock);
  switch (type->category()) {
  case Type::Category::Contract:
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
          llvm::dyn_cast<iele::IeleLocalVariable>(Bswapped), ArgTypeSize, ArgValue,
          CompilingBlock);
      } else {
        Bswapped = ArgValue;
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
      appendByteWidth(ArgLen, ArgValue);
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
          llvm::dyn_cast<iele::IeleLocalVariable>(Bswapped), ArgLen, ArgValue,
          CompilingBlock);
      } else {
        Bswapped = ArgValue;
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
          ArgLen, ArgValue,
          CompilingBlock) :
        iele::IeleInstruction::CreateLoad(
          ArgLen, ArgValue,
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
        iele::IeleInstruction::Add, StringAddress, ArgValue,
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
            ArrayLen, ArgValue,
            CompilingBlock) :
          iele::IeleInstruction::CreateLoad(
            ArrayLen, ArgValue,
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
          Element, ArgValue, 
          iele::IeleIntConstant::getOne(&Context),
          CompilingBlock);
      } else {
        iele::IeleInstruction::CreateAssign(
          ArrayLen,
          iele::IeleIntConstant::Create(&Context, arrayType.length()),
          CompilingBlock);
        iele::IeleInstruction::CreateAssign(
          Element, ArgValue, CompilingBlock);
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
      case DataLocation::Memory: {
        elementSize = elementType->memorySize();
        break;
      }
      case DataLocation::CallData:
        solAssert(false, "not supported by IELE.");
      }
  
      elementType = ReferenceType::copyForLocationIfReference(arrayType.location(), elementType);
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
    for (auto decl : structType.structDefinition().members()) {
      // First compute the offset from the start of the struct.
      bigint offset;
      switch (structType.location()) {
      case DataLocation::Storage: {
        const auto& offsets = structType.storageOffsetsOfMember(decl->name());
        offset = offsets.first;
        break;
      }
      case DataLocation::Memory: {
        offset = structType.memoryOffsetOfMember(decl->name());
        break;
      case DataLocation::CallData: 
        solAssert(false, "Call data not supported in IELE");
      }
      }

      iele::IeleLocalVariable *Member =
        iele::IeleLocalVariable::Create(&Context, "encode.address", CompilingFunction);
      iele::IeleValue *OffsetValue =
        iele::IeleIntConstant::Create(&Context, offset);
      iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Add, Member, ArgValue, OffsetValue,
        CompilingBlock);

      TypePointer memberType = ReferenceType::copyForLocationIfReference(structType.location(), decl->type());
      doEncode(NextFree, CrntPos, makeLValue(Member, memberType, structType.location()), ArgTypeSize, ArgLen, memberType, appendWidths, bigEndian);
    }
    break;
  }
  case Type::Category::Function:
      solAssert(false, 
                "IeleCompiler: encoding of given type not implemented yet");
  default:
      solAssert(false, "IeleCompiler: invalid type for encoding");
  }
}

iele::IeleValue *IeleCompiler::decoding(iele::IeleValue *encoded, TypePointer type) {
  if (type->isValueType() || type->category() == Type::Category::Mapping) {
    return encoded;
  }

  // Allocate cell 
  iele::IeleLocalVariable *NextFree = appendMemorySpill();

  iele::IeleInstruction::CreateStore(
    encoded, NextFree, CompilingBlock);

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
  doDecode(NextFree, CrntPos, RegisterLValue::Create(Result), ArgTypeSize, ArgLen, type);
  return Result;
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
      switch(type->category()) {
      case Type::Category::Integer: {
        IntegerType const& intType = dynamic_cast<const IntegerType &>(*type);
        if (intType.isSigned()) {
          break;
        } else {
          // fall through
	}
      }
      case Type::Category::Contract:
      case Type::Category::FixedBytes:
      case Type::Category::Enum:
        iele::IeleInstruction::CreateBinOp(
          iele::IeleInstruction::Twos,
          llvm::dyn_cast<iele::IeleLocalVariable>(AllocedValue), ArgTypeSize, AllocedValue,
          CompilingBlock);
        break;
      case Type::Category::Bool:
       break;
      default:
        solAssert(false, "invalid type");
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
        iele::IeleInstruction::Add, CrntPos, CrntPos, ArgLen, CompilingBlock);
    }
    appendRangeCheck(AllocedValue, *type);
    StoreAt->write(AllocedValue, CompilingBlock);
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
        StoreAt->write(AllocedValue, CompilingBlock);
      } else {
        AllocedValue = StoreAt->read(CompilingBlock);
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
        iele::IeleInstruction::BSwap, 
        StringValue, ArgLen, StringValue,
        CompilingBlock);
      iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Add, CrntPos, CrntPos, ArgLen, CompilingBlock);
      AddressLValue::Create(this, AllocedValue, DataLocation::Memory)->write(ArgLen, CompilingBlock);

      iele::IeleLocalVariable *Offset =
        iele::IeleLocalVariable::Create(&Context, "value.address", CompilingFunction);
      iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Add, Offset, AllocedValue, iele::IeleIntConstant::getOne(&Context),
        CompilingBlock);
      AddressLValue::Create(this, Offset, DataLocation::Memory)->write(StringValue, CompilingBlock);
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
        StoreAt->write(AllocedValue, CompilingBlock);
  
        iele::IeleInstruction::CreateBinOp(
          iele::IeleInstruction::Add,
          Element, AllocedValue, 
          iele::IeleIntConstant::getOne(&Context),
          CompilingBlock);
      } else {
        if (dynamic_cast<RegisterLValue *>(StoreAt)) {
          AllocedValue = appendArrayAllocation(arrayType);
          StoreAt->write(AllocedValue, CompilingBlock);
        } else {
          AllocedValue = StoreAt->read(CompilingBlock);
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
      case DataLocation::Memory: {
        elementSize = elementType->memorySize();
        break;
      }
      case DataLocation::CallData:
        solAssert(false, "not supported by IELE.");
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
      StoreAt->write(AllocedValue, CompilingBlock);
    } else {
      AllocedValue = StoreAt->read(CompilingBlock);
    }

    for (auto decl : structType.structDefinition().members()) {
      // First compute the offset from the start of the struct.
      bigint offset;
      switch (structType.location()) {
      case DataLocation::Storage: {
        const auto& offsets = structType.storageOffsetsOfMember(decl->name());
        offset = offsets.first;
        break;
      }
      case DataLocation::Memory: {
        offset = structType.memoryOffsetOfMember(decl->name());
        break;
      case DataLocation::CallData: 
        solAssert(false, "Call data not supported in IELE");
      }
      }

      iele::IeleLocalVariable *Member =
        iele::IeleLocalVariable::Create(&Context, "encode.address", CompilingFunction);
      iele::IeleValue *OffsetValue =
        iele::IeleIntConstant::Create(&Context, offset);
      iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Add, Member, AllocedValue, OffsetValue,
        CompilingBlock);

      doDecode(NextFree, CrntPos, makeLValue(Member, decl->type(), DataLocation::Memory), ArgTypeSize, ArgLen, decl->type());
    }
    break;
  }
  default: 
    solAssert(false, "invalid reference type");
    break;
  }
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
      functionCall.arguments().front()->annotation().type,
      functionCall.annotation().type);
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
              structType.memoryMemberTypes().size(),
              "IeleCompiler: struct constructor with missing arguments");

    // Allocate memory for the struct.
    iele::IeleValue *StructValue = appendStructAllocation(structType);

    // Visit arguments and initialize struct fields.
    bigint offset = 0; 
    for (unsigned i = 0; i < arguments.size(); ++i) {
      // Visit argument.
      iele::IeleValue *InitValue = compileExpression(*arguments[i]);
      TypePointer argType = arguments[i]->annotation().type;
      TypePointer memberType = functionType->parameterTypes()[i];
      InitValue = appendTypeConversion(InitValue, argType, memberType);

      // Address in the new struct in which we should copy the argument value.
      iele::IeleLocalVariable *AddressValue =
        iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
      iele::IeleInstruction::CreateBinOp(
          iele::IeleInstruction::Add, AddressValue, StructValue,
          iele::IeleIntConstant::Create(&Context, offset), CompilingBlock);

      // Size of copy.
      bigint memberSize = memberType->memorySize();
      iele::IeleValue *SizeValue =
        iele::IeleIntConstant::Create(&Context, memberSize);

      // Do the copy.
      solAssert(!memberType->dataStoredIn(DataLocation::Storage),
                "IeleCompiler: found init value for struct member in storage "
                "after type conversion");
      if (memberType->dataStoredIn(DataLocation::Memory))
        appendIeleRuntimeCopy(InitValue, AddressValue, SizeValue,
                              DataLocation::Memory, DataLocation::Memory);
      else
        iele::IeleInstruction::CreateStore(InitValue, AddressValue,
                                           CompilingBlock);

      offset += memberSize;
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
      arguments.front()->annotation().type,
      function.parameterTypes().front());
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
      CompilingFunctionStatus;
    iele::IeleInstruction::CreateAccountCall(false, StatusValue, EmptyLValues, Deposit,
                                             TargetAddressValue,
                                             TransferValue, GasValue,
                                             EmptyArguments,
                                             CompilingBlock);
    
    if (function.kind() == FunctionType::Kind::Transfer) {
      // For transfer revert if status is not zero.
      appendRevert(StatusValue, StatusValue);
    } else {
      iele::IeleLocalVariable *ResultValue =
        iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
      iele::IeleInstruction::CreateIsZero(ResultValue, StatusValue, CompilingBlock);
      CompilingExpressionResult.push_back(ResultValue);
    }
    break;
  }
  case FunctionType::Kind::Revert: {
      iele::IeleValue *Message = nullptr;
      if (arguments.size() > 1) {
        Message = compileExpression(*arguments[0]);
      }
      appendRevert(nullptr, Message);
      break;
  }
  case FunctionType::Kind::Assert:
  case FunctionType::Kind::Require: {
    // Visit condition.
    iele::IeleValue *ConditionValue =
      compileExpression(*arguments.front().get());
    ConditionValue = appendTypeConversion(ConditionValue,
      arguments.front()->annotation().type,
      function.parameterTypes().front());
    solAssert(ConditionValue,
           "IeleCompiler: Failed to compile require/assert condition.");

    // Create check for false.
    iele::IeleLocalVariable *InvConditionValue =
      iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
    iele::IeleInstruction::CreateIsZero(InvConditionValue, ConditionValue,
                                        CompilingBlock);
    if (function.kind() == FunctionType::Kind::Assert)
      appendInvalid(InvConditionValue);
    else {
      iele::IeleValue *Message = nullptr;
      if (arguments.size() > 1) {
        Message = compileExpression(*arguments[1]);
      }
      appendRevert(InvConditionValue, Message);
    }
    break;
  }
  case FunctionType::Kind::AddMod: {
    iele::IeleValue *Op1 = compileExpression(*arguments[0].get());
    Op1 = appendTypeConversion(Op1, arguments[0]->annotation().type, UInt);
    solAssert(Op1,
           "IeleCompiler: Failed to compile operand 1 of addmod.");
    iele::IeleValue *Op2 = compileExpression(*arguments[1].get());
    Op2 = appendTypeConversion(Op2, arguments[1]->annotation().type, UInt);
    solAssert(Op2,
           "IeleCompiler: Failed to compile operand 2 of addmod.");
    iele::IeleValue *Modulus = compileExpression(*arguments[2].get());
    Modulus = appendTypeConversion(Modulus, arguments[2]->annotation().type, UInt);
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
    Op1 = appendTypeConversion(Op1, arguments[0]->annotation().type, UInt);
    solAssert(Op1,
           "IeleCompiler: Failed to compile operand 1 of addmod.");
    iele::IeleValue *Op2 = compileExpression(*arguments[1].get());
    Op2 = appendTypeConversion(Op2, arguments[1]->annotation().type, UInt);
    solAssert(Op2,
           "IeleCompiler: Failed to compile operand 2 of addmod.");
    iele::IeleValue *Modulus = compileExpression(*arguments[2].get());
    Modulus = appendTypeConversion(Modulus, arguments[2]->annotation().type, UInt);
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
    CompilingExpressionResult.push_back(ArrayValue);
    break;
  }
  case FunctionType::Kind::DelegateCall:
  case FunctionType::Kind::Internal: {
    bool isDelegateCall = function.kind() == FunctionType::Kind::DelegateCall;
    // Visit arguments.
    llvm::SmallVector<iele::IeleValue *, 4> Arguments;
    llvm::SmallVector<iele::IeleLocalVariable *, 4> Returns;
    compileFunctionArguments(&Arguments, &Returns, arguments, function, false);

    // Visit callee.
    llvm::SmallVector<iele::IeleValue*, 2> CalleeValues;
    compileTuple(functionCall.expression(), CalleeValues);
    iele::IeleGlobalValue *CalleeValue;
    if (CalleeValues.size() == 1) {
      CalleeValue = llvm::dyn_cast<iele::IeleGlobalValue>(CalleeValues[0]);
    } else {
      solAssert(CalleeValues.size() == 2, "Invalid number of results from function call expression");
      CalleeValue = llvm::dyn_cast<iele::IeleGlobalValue>(CalleeValues[1]);
      Arguments.insert(Arguments.begin(), CalleeValues[0]);
    }
    if (hasTwoFunctions(function, false, isDelegateCall)) {
      CalleeValue = convertFunctionToInternal(CalleeValue);
    }
    solAssert(CalleeValue,
              "IeleCompiler: Failed to compile callee of internal function "
              "call");

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
      arguments.front()->annotation().type,
      function.parameterTypes().front());
    solAssert(TargetAddress,
              "IeleCompiler: Failed to compile Selfdestruct target.");

    // Make IELE instruction
    iele::IeleInstruction::CreateSelfdestruct(TargetAddress, CompilingBlock);
    break;
  }
  case FunctionType::Kind::Event: {
    llvm::SmallVector<iele::IeleValue *, 4> IndexedArguments;
    llvm::SmallVector<iele::IeleValue *, 4> NonIndexedArguments;
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
          bigint(u256(h256::Arith(dev::keccak256(
            function.externalSignature())))),
          true /* print as hex */);
      IndexedArguments.push_back(EventID);
      ++numIndexed;
    }

    // Visit indexed arguments.
    for (unsigned arg = 0; arg < arguments.size(); ++arg) {
      if (event.parameters()[arg]->isIndexed()) {
        numIndexed++;
        // Compile argument
        iele::IeleValue *ArgValue = compileExpression(*arguments[arg]);
        solAssert(ArgValue,
                  "IeleCompiler: Failed to compile indexed event argument ");
        auto ArgType = arguments[arg]->annotation().type;
        auto ParamType = function.parameterTypes()[arg];
        ArgValue = appendTypeConversion(ArgValue, ArgType, ParamType);
        ArgValue = encoding(ArgValue, ParamType, true);
        IndexedArguments.push_back(ArgValue);
      }
    }

    // Max 4 indexed params allowed! 
    solAssert(numIndexed <= 4, "Too many indexed arguments.");
    
    // Visit non indexed params
    for (unsigned arg = 0; arg < arguments.size(); ++arg) {
      if (!event.parameters()[arg]->isIndexed()) {
        // Compile argument
        iele::IeleValue *ArgValue = compileExpression(*arguments[arg]);
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
    llvm::SmallVector<iele::IeleValue*, 2> CalleeValues;
    compileTuple(functionCall.expression(), CalleeValues);

    GasValue = compileExpression(*arguments.front());
    GasValue = appendTypeConversion(GasValue, arguments.front()->annotation().type, UInt);
    CompilingExpressionResult.insert(
        CompilingExpressionResult.end(), CalleeValues.begin(), CalleeValues.end());
    break;
  }
  case FunctionType::Kind::SetValue: {
    llvm::SmallVector<iele::IeleValue*, 2> CalleeValues;
    compileTuple(functionCall.expression(), CalleeValues);

    TransferValue = compileExpression(*arguments.front());
    TransferValue = appendTypeConversion(TransferValue, arguments.front()->annotation().type, UInt);
    CompilingExpressionResult.insert(
        CompilingExpressionResult.end(), CalleeValues.begin(), CalleeValues.end());
    break;
  }
  case FunctionType::Kind::External: {
    // Visit arguments.
    llvm::SmallVector<iele::IeleValue *, 4> Arguments;
    llvm::SmallVector<iele::IeleLocalVariable *, 4> Returns;
    compileFunctionArguments(&Arguments, &Returns, arguments, function, true);

    llvm::SmallVector<iele::IeleValue*, 2> CalleeValues;
    compileExpressions(functionCall.expression(), CalleeValues, 2);
    iele::IeleGlobalValue *FunctionCalleeValue =
      llvm::dyn_cast<iele::IeleGlobalValue>(CalleeValues[1]);
    iele::IeleValue *AddressValue =
      CalleeValues[0];

    iele::IeleLocalVariable *StatusValue =
      CompilingFunctionStatus;

    bool StaticCall =
      function.stateMutability() <= StateMutability::View;

    solAssert(!StaticCall || !function.valueSet(), "Value set for staticcall");

    if (!StaticCall && !function.valueSet()) {
      TransferValue = iele::IeleIntConstant::getZero(&Context);
    }

    if (!function.gasSet()) {
      llvm::SmallVector<iele::IeleValue *, 0> EmptyArguments;
      iele::IeleLocalVariable *GasValue =
        iele::IeleLocalVariable::Create(&Context, "gas", CompilingFunction);
      iele::IeleInstruction::CreateIntrinsicCall(
        iele::IeleInstruction::Gas, GasValue, EmptyArguments,
        CompilingBlock);
      this->GasValue = GasValue;
    }

    iele::IeleInstruction::CreateAccountCall(
      StaticCall, StatusValue, Returns, FunctionCalleeValue,
      AddressValue, TransferValue, GasValue, Arguments,
      CompilingBlock);

    appendRevert(StatusValue, StatusValue);

    llvm::SmallVector<iele::IeleValue *, 4> DecodedReturns;
    for (unsigned i = 0; i < Returns.size(); i++) {
      iele::IeleLocalVariable *Return = Returns[i];
      TypePointer type = function.returnParameterTypes()[i];
      DecodedReturns.push_back(decoding(Return, type));
    }

    CompilingExpressionResult.insert(
        CompilingExpressionResult.end(), DecodedReturns.begin(), DecodedReturns.end());
    TransferValue = nullptr;
    GasValue = nullptr;
    break;
  }
  case FunctionType::Kind::CallCode:
  case FunctionType::Kind::BareCall:
  case FunctionType::Kind::BareCallCode:
  case FunctionType::Kind::BareDelegateCall:
    solAssert(false, "low-level function calls not supported in IELE");
  case FunctionType::Kind::Creation: {
    solAssert(!function.gasSet(), "Gas limit set for contract creation.");
    llvm::SmallVector<iele::IeleValue *, 4> Arguments;
    llvm::SmallVector<iele::IeleLocalVariable *, 1> Returns;
    compileFunctionArguments(&Arguments, &Returns, arguments, function, true);
    solAssert(Returns.size() == 1, "New expression returns a single result.");

    iele::IeleValue *Callee = compileExpression(functionCall.expression());
    auto Contract = dynamic_cast<iele::IeleContract *>(Callee);
    solAssert(Contract, "invalid value passed to contract creation");

    iele::IeleLocalVariable *StatusValue =
      CompilingFunctionStatus;

    if (!function.valueSet()) {
      TransferValue = iele::IeleIntConstant::getZero(&Context);
    }

    iele::IeleInstruction::CreateCreate(
      false, StatusValue, Returns[0], Contract, nullptr,
      TransferValue, Arguments, CompilingBlock);

    appendRevert(StatusValue, StatusValue);

    CompilingExpressionResult.insert(
        CompilingExpressionResult.end(), Returns.begin(), Returns.end());
    TransferValue = nullptr;
    GasValue = nullptr;
    break;
  }
  case FunctionType::Kind::Log0:
  case FunctionType::Kind::Log1:
  case FunctionType::Kind::Log2:
  case FunctionType::Kind::Log3:
  case FunctionType::Kind::Log4: {
    llvm::SmallVector<iele::IeleValue *, 4> CompiledArguments;

    // First arg is the data part
    iele::IeleValue *DataArg = compileExpression(*arguments[0]);
    solAssert(DataArg,
              "IeleCompiler: Failed to compile data argument of log operation ");

    // Remaining args are the topics
    for (unsigned arg = 1; arg < arguments.size(); arg++)
    {
        iele::IeleValue *ArgValue = compileExpression(*arguments[arg]);
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
  case FunctionType::Kind::ECRecover: {
    // Visit arguments.
    llvm::SmallVector<iele::IeleValue *, 4> Arguments;
    llvm::SmallVector<iele::IeleLocalVariable *, 1> Returns;
    compileFunctionArguments(&Arguments, &Returns, arguments, function, true);

    iele::IeleGlobalVariable *FunctionCalleeValue =
      iele::IeleGlobalVariable::Create(&Context, "iele.ecrec");
    iele::IeleValue *AddressValue =
      iele::IeleIntConstant::getOne(&Context);

    iele::IeleLocalVariable *StatusValue =
      CompilingFunctionStatus;

    if (!function.gasSet()) {
      llvm::SmallVector<iele::IeleValue *, 0> EmptyArguments;
      iele::IeleLocalVariable *GasValue =
        iele::IeleLocalVariable::Create(&Context, "gas", CompilingFunction);
      iele::IeleInstruction::CreateIntrinsicCall(
        iele::IeleInstruction::Gas, GasValue, EmptyArguments,
        CompilingBlock);
      this->GasValue = GasValue;
    }

    iele::IeleInstruction::CreateAccountCall(
      true, StatusValue, Returns, FunctionCalleeValue, AddressValue,
      nullptr, GasValue, Arguments, CompilingBlock);

    appendRevert(StatusValue, StatusValue);
    iele::IeleLocalVariable *Failed =
      iele::IeleLocalVariable::Create(&Context, "ecrec.failed", CompilingFunction);
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::CmpEq, Failed, Returns[0],
      iele::IeleIntConstant::getMinusOne(&Context),
      CompilingBlock);
    appendRevert(Failed);

    CompilingExpressionResult.insert(
        CompilingExpressionResult.end(), Returns.begin(), Returns.end());
    TransferValue = nullptr;
    GasValue = nullptr;
    break;
  }
  case FunctionType::Kind::BlockHash: {
    iele::IeleValue *BlockNum = compileExpression(*arguments.front());
    BlockNum = appendTypeConversion(BlockNum, arguments.front()->annotation().type, function.parameterTypes()[0]);
    llvm::SmallVector<iele::IeleValue *, 0> Arguments(1, BlockNum);
    iele::IeleLocalVariable *BlockHashValue =
      iele::IeleLocalVariable::Create(&Context, "blockhash", CompilingFunction);
    iele::IeleInstruction::CreateIntrinsicCall(
      iele::IeleInstruction::Blockhash, BlockHashValue, Arguments,
      CompilingBlock);
    CompilingExpressionResult.push_back(BlockHashValue);
    break;
  }
  case FunctionType::Kind::SHA3:
  case FunctionType::Kind::SHA256:
  case FunctionType::Kind::RIPEMD160: {
    // Visit arguments
    llvm::SmallVector<iele::IeleValue *, 4> Arguments;
    TypePointers Types;
    for (unsigned i = 0; i < arguments.size(); ++i) {
      iele::IeleValue *ArgValue = compileExpression(*arguments[i]);
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
    if (function.kind() == FunctionType::Kind::SHA3) {
      iele::IeleInstruction::CreateSha3(
        Return, NextFree, CompilingBlock);
    } else {
      iele::IeleLocalVariable *ArgValue =
        iele::IeleLocalVariable::Create(&Context, "encoded.val", CompilingFunction);
      iele::IeleInstruction::CreateLoad(
        ArgValue, NextFree, iele::IeleIntConstant::getZero(&Context), ByteWidth, CompilingBlock);
      iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Twos,
        ArgValue, ByteWidth, ArgValue,
        CompilingBlock);
      iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::BSwap,
        ArgValue, ByteWidth, ArgValue,
        CompilingBlock);
      Arguments.clear();
      Arguments.push_back(ByteWidth);
      Arguments.push_back(ArgValue);

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
 
      if (!function.gasSet()) {
        llvm::SmallVector<iele::IeleValue *, 0> EmptyArguments;
        iele::IeleLocalVariable *GasValue =
          iele::IeleLocalVariable::Create(&Context, "gas", CompilingFunction);
        iele::IeleInstruction::CreateIntrinsicCall(
          iele::IeleInstruction::Gas, GasValue, EmptyArguments,
          CompilingBlock);
        this->GasValue = GasValue;
      }

      iele::IeleInstruction::CreateAccountCall(
        true, StatusValue, Returns, FunctionCalleeValue, AddressValue,
        nullptr, GasValue, Arguments, CompilingBlock);

      appendRevert(StatusValue, StatusValue);
      TransferValue = nullptr;
      GasValue = nullptr;
    }
    CompilingExpressionResult.push_back(Return);
    break;
  }
  case FunctionType::Kind::ByteArrayPush: {
    iele::IeleValue *PushedValue = compileExpression(*arguments.front());
    iele::IeleValue *ArrayValue = compileExpression(functionCall.expression());
    IeleLValue *SizeLValue = AddressLValue::Create(this, ArrayValue, CompilingLValueArrayType->location());
    iele::IeleValue *SizeValue = SizeLValue->read(CompilingBlock);
    iele::IeleLocalVariable *StringAddress =
      iele::IeleLocalVariable::Create(&Context, "string.address", CompilingFunction);
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Add, StringAddress, ArrayValue,
      iele::IeleIntConstant::getOne(&Context),
      CompilingBlock);
    IeleLValue *ElementLValue = ByteArrayLValue::Create(this, StringAddress, SizeValue, CompilingLValueArrayType->location());
    ElementLValue->write(PushedValue, CompilingBlock);

    iele::IeleLocalVariable *NewSize =
      iele::IeleLocalVariable::Create(&Context, "array.new.length", CompilingFunction);
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Add, NewSize, SizeValue,
      iele::IeleIntConstant::getOne(&Context),
      CompilingBlock);

    SizeLValue->write(NewSize, CompilingBlock);
    CompilingExpressionResult.push_back(NewSize);
    break;
  }
  case FunctionType::Kind::ArrayPush: {
    iele::IeleValue *PushedValue = compileExpression(*arguments.front());
    iele::IeleValue *ArrayValue = compileExpression(functionCall.expression());
    IeleLValue *SizeLValue = AddressLValue::Create(this, ArrayValue, CompilingLValueArrayType->location());
    iele::IeleValue *SizeValue = SizeLValue->read(CompilingBlock);

    TypePointer elementType = CompilingLValueArrayType->baseType();
    TypePointer RHSType = arguments.front()->annotation().type;

    bigint elementSize;
    switch (CompilingLValueArrayType->location()) {
    case DataLocation::Storage: {
      elementSize = elementType->storageSize();
      break;
    }
    case DataLocation::Memory: {
      elementSize = elementType->memorySize();
      break;
    }
    case DataLocation::CallData:
      solAssert(false, "not supported by IELE.");
    }
 
    iele::IeleValue *ElementSizeValue =
      iele::IeleIntConstant::Create(&Context, elementSize);
 
    iele::IeleLocalVariable *AddressValue =
      iele::IeleLocalVariable::Create(&Context, "array.last.element", CompilingFunction);
    // compute the data offset of the last element
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Mul, AddressValue, SizeValue, ElementSizeValue,
      CompilingBlock);
    // add one for the length slot
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Add, AddressValue, AddressValue,
      iele::IeleIntConstant::getOne(&Context),
      CompilingBlock);
    // compute the address of the last element
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Add, AddressValue, AddressValue, ArrayValue,
      CompilingBlock);

    IeleLValue *ElementLValue = makeLValue(AddressValue, elementType, CompilingLValueArrayType->location());
    if (shouldCopyStorageToStorage(ElementLValue, *RHSType))
      appendCopyFromStorageToStorage(ElementLValue, elementType, PushedValue, RHSType);
    else if (shouldCopyMemoryToStorage(*elementType, ElementLValue, *RHSType))
      appendCopyFromMemoryToStorage(ElementLValue, elementType, PushedValue, RHSType);
    else if (shouldCopyMemoryToMemory(*elementType, ElementLValue, *RHSType))
      appendCopyFromMemoryToMemory(ElementLValue, elementType, PushedValue, RHSType);
    else
      ElementLValue->write(PushedValue, CompilingBlock);

    iele::IeleLocalVariable *NewSize =
      iele::IeleLocalVariable::Create(&Context, "array.new.length", CompilingFunction);
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Add, NewSize, SizeValue,
      iele::IeleIntConstant::getOne(&Context),
      CompilingBlock);

    SizeLValue->write(NewSize, CompilingBlock);
    CompilingExpressionResult.push_back(NewSize);
    break;
  }
  case FunctionType::Kind::ABIEncode:
  case FunctionType::Kind::ABIEncodePacked: {
    llvm::SmallVector<iele::IeleValue *, 4> Arguments;
    TypePointers Types;
    for (unsigned i = 0; i < arguments.size(); ++i) {
      iele::IeleValue *ArgValue = compileExpression(*arguments[i]);
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

    CompilingExpressionResult.push_back(Return);
    break;
  }
  default:
      solAssert(false, "IeleCompiler: Invalid function type.");
  }

  return false;
}

template <class ArgClass, class ReturnClass, class ExpressionClass>
void IeleCompiler::compileFunctionArguments(ArgClass *Arguments, ReturnClass *Returns, 
      const std::vector<ASTPointer<ExpressionClass>> &arguments, const FunctionType &function, bool encode) {
    for (unsigned i = 0; i < arguments.size(); ++i) {
      iele::IeleValue *ArgValue = compileExpression(*arguments[i]);
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
      Arguments->push_back(ArgValue);
    }

    // Prepare registers for return values.
    for (unsigned i = 0; i < function.returnParameterTypes().size(); ++i)
      Returns->push_back(
        iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction));
}

bool IeleCompiler::visit(const NewExpression &newExpression) {
  const Type &Type = *newExpression.typeName().annotation().type;
  const ContractType *contractType = dynamic_cast<const ContractType *>(&Type);
  solAssert(contractType, "not implemented yet");
  const ContractDefinition &ContractDefinition = contractType->contractDefinition();
  iele::IeleContract *Contract = CompiledContracts[&ContractDefinition];
  std::vector<iele::IeleContract *> &contracts = CompilingContract->getIeleContractList();
  if (std::find(contracts.begin(), contracts.end(), Contract) == contracts.end()) {
    CompilingContract->getIeleContractList().push_back(Contract);
  }
  solAssert(Contract, "Could not find compiled contract for new expression");
  CompilingExpressionResult.push_back(Contract);
  return false;
}

bool IeleCompiler::visit(const MemberAccess &memberAccess) {
   const std::string& member = memberAccess.memberName();

  // Not supported yet cases.
  if (const FunctionType *funcType =
        dynamic_cast<const FunctionType *>(
            memberAccess.annotation().type.get())) {
    if (funcType->bound()) {
      iele::IeleValue *boundValue = compileExpression(memberAccess.expression());
      boundValue = appendTypeConversion(boundValue, 
        memberAccess.expression().annotation().type,
        funcType->selfType());
      CompilingExpressionResult.push_back(boundValue);
      
      const Declaration *declaration =
        memberAccess.annotation().referencedDeclaration;
      const FunctionDefinition *functionDef =
            dynamic_cast<const FunctionDefinition *>(declaration);
      solAssert(functionDef, "IeleCompiler: bound function does not have a definition");
      std::string name = getIeleNameForFunction(*functionDef); 

      // Lookup identifier in the function's symbol table.
      iele::IeleValueSymbolTable *ST = CompilingFunction->getIeleValueSymbolTable();
      solAssert(ST,
                "IeleCompiler: failed to access compiling function's symbol "
                "table.");
      if (iele::IeleValue *Identifier =
            ST->lookup(VarNameMap[ModifierDepth][name])) {
        CompilingExpressionResult.push_back(RegisterLValue::Create(llvm::dyn_cast<iele::IeleLocalVariable>(Identifier)));
        return false;
      }
      ST = CompilingContract->getIeleValueSymbolTable();
      solAssert(ST,
               "IeleCompiler: failed to access compiling contract's symbol "
               "table.");
      if (iele::IeleValue *Identifier = ST->lookup(name)) {
        appendVariable(Identifier, functionDef->name(), true);
        return false;
      }

      solAssert(false, "Invalid bound function without declaration");
      return false;
    }
  }

  const Type *actualType = memberAccess.expression().annotation().type.get();
  if (const TypeType *type = dynamic_cast<const TypeType *>(actualType)) {
    if (dynamic_cast<const ContractType *>(type->actualType().get())) {
      iele::IeleValueSymbolTable *ST = CompilingContract->getIeleValueSymbolTable();
      solAssert(ST,
                "IeleCompiler: failed to access compiling contract's symbol table.");
      if (auto funType = dynamic_cast<const FunctionType *>(memberAccess.annotation().type.get())) {
        switch(funType->kind()) {
        case FunctionType::Kind::DelegateCall:
        case FunctionType::Kind::Internal:
	  if (const auto * function = dynamic_cast<const FunctionDefinition *>(memberAccess.annotation().referencedDeclaration)) {
            std::string name = getIeleNameForFunction(*function);
            iele::IeleValue *Result = ST->lookup(name);
            CompilingExpressionResult.push_back(Result);
            return false;
          } else {
            solAssert(false, "Function member not found");
          }
        case FunctionType::Kind::External:
        case FunctionType::Kind::Creation:
        case FunctionType::Kind::Send:
        case FunctionType::Kind::Transfer:
          // handled below
          actualType = type->actualType().get();
          break;
        case FunctionType::Kind::BareCall:
        case FunctionType::Kind::BareCallCode:
        case FunctionType::Kind::BareDelegateCall:
        case FunctionType::Kind::CallCode:
        default:
          solAssert(false, "not implemented yet");
        }
      } else if (dynamic_cast<const TypeType *>(memberAccess.annotation().type.get())) {
        return false;
        //noop
      } else if (auto variable = dynamic_cast<const VariableDeclaration *>(memberAccess.annotation().referencedDeclaration)) {
        if (variable->isConstant()) {
          iele::IeleValue *Result = compileExpression(*variable->value());
          TypePointer rhsType = variable->value()->annotation().type;
          Result = appendTypeConversion(Result, rhsType, variable->annotation().type);
          CompilingExpressionResult.push_back(Result);
        } else {
          std::string name = getIeleNameForStateVariable(variable);
          iele::IeleValue *Result = ST->lookup(name);
          solAssert(Result, "IeleCompiler: failed to find state variable in "
                            "contract's symbol table");
          appendVariable(Result, variable->name(), variable->annotation().type->isValueType());
        }
        return false;
      } else {
        solAssert(false, "not implemented yet");
      }
    } else if (auto enumType = dynamic_cast<const EnumType *>(type->actualType().get())) {
      iele::IeleIntConstant *Result = iele::IeleIntConstant::Create(&Context, bigint(enumType->memberValue(memberAccess.memberName())));
      CompilingExpressionResult.push_back(Result);
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
      CompilingExpressionResult.push_back(Result);
      return false;
    } else {
      if (const Declaration *declaration = memberAccess.annotation().referencedDeclaration) {
        memberAccess.expression().accept(*this);
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
        iele::IeleValue *Result = iele::IeleFunction::Create(&Context, true, name);
        CompilingExpressionResult.push_back(Result);
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
    // Visit accessed exression (skip in case of magic base expression).
    iele::IeleValue *ExprValue = compileExpression(memberAccess.expression());
    solAssert(ExprValue,
              "IeleCompiler: failed to compile base expression for member "
              "access.");
    solAssert(!(std::set<std::string>{"call", "callcode", "delegatecall"}).count(member),
              "IeleCompiler: member not supported in IELE");
    if (member == "transfer" || member == "send") {
      ExprValue = appendTypeConversion(ExprValue,
        memberAccess.expression().annotation().type,
        Address);
      solAssert(ExprValue, "IeleCompiler: Failed to compile address");
      CompilingExpressionResult.push_back(ExprValue);
    } else if (member == "balance") {
      ExprValue = appendTypeConversion(ExprValue,
        memberAccess.expression().annotation().type,
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
    if (member == "value" || member == "gas") {
      llvm::SmallVector<iele::IeleValue*, 2> CalleeValues;
      compileTuple(memberAccess.expression(), CalleeValues);
      CompilingExpressionResult.insert(
          CompilingExpressionResult.end(), CalleeValues.begin(), CalleeValues.end());
    }
    else if (member == "selector")
      solAssert(false, "IeleCompiler: member not supported in IELE");
    else
      solAssert(false, "IeleCompiler: invalid member for function value");
    break;
  case Type::Category::Struct: {
    const StructType &type = dynamic_cast<const StructType &>(baseType);

    // Visit accessed exression (skip in case of magic base expression).
    iele::IeleValue *ExprValue = compileExpression(memberAccess.expression());
    solAssert(ExprValue,
              "IeleCompiler: failed to compile base expression for member "
              "access.");

    IeleLValue *AddressValue = appendStructAccess(type, ExprValue, member, type.location());
    CompilingExpressionResult.push_back(AddressValue);
    break;
  }
  case Type::Category::FixedBytes: {
    // Visit accessed exression (skip in case of magic base expression).
    iele::IeleValue *ExprValue = compileExpression(memberAccess.expression());
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
    CompilingExpressionResult.push_back(LengthValue);
    break;
  }
  case Type::Category::Array: {
    const ArrayType &type = dynamic_cast<const ArrayType &>(baseType);
    iele::IeleValue *ExprValue = compileExpression(memberAccess.expression());

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
    case DataLocation::Memory: {
      elementSize = elementType.memorySize();
      break;
    }
    case DataLocation::CallData:
      solAssert(false, "not supported by IELE.");
    }
 
    if (member == "length") {
      if (type.isDynamicallySized()) {
        // If the array is dynamically sized the size is stored in the first slot.
        CompilingExpressionResult.push_back(ArrayLengthLValue::Create(this, ExprValue, type.location()));
        CompilingLValueArrayType = &type;
      } else {
        CompilingExpressionResult.push_back(
          iele::IeleIntConstant::Create(&Context, type.length()));
      }
    } else if (member == "push") {
      solAssert(type.isDynamicallySized(), "Invalid push on fixed length array");
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
    iele::IeleValue *ExprValue = compileExpression(indexAccess.baseExpression());
    solAssert(ExprValue,
              "IeleCompiler: failed to compile base expression for index "
              "access.");

    // Visit index expression.
    solAssert(indexAccess.indexExpression(),
              "IeleCompiler: Index expression expected.");
    iele::IeleValue *IndexValue =
      compileExpression(*indexAccess.indexExpression());
    IndexValue =
      appendTypeConversion(
          IndexValue,
          indexAccess.indexExpression()->annotation().type, UInt);
    solAssert(IndexValue,
              "IeleCompiler: failed to compile index expression for index "
              "access.");

    IeleLValue *AddressValue = appendArrayAccess(type, IndexValue, ExprValue, type.location());
    CompilingExpressionResult.push_back(AddressValue);
    break;
  }
  case Type::Category::FixedBytes: {
    const FixedBytesType &type = dynamic_cast<const FixedBytesType &>(baseType);

    // Visit accessed exression.
    iele::IeleValue *ExprValue = compileExpression(indexAccess.baseExpression());
    solAssert(ExprValue,
              "IeleCompiler: failed to compile base expression for index "
              "access.");

    solAssert(indexAccess.indexExpression(),
              "IeleCompiler: Index expression expected.");

     // Visit index expression.
    iele::IeleValue *IndexValue =
      compileExpression(*indexAccess.indexExpression());
    IndexValue = appendTypeConversion(IndexValue, indexAccess.indexExpression()->annotation().type, UInt);
    solAssert(IndexValue,
              "IeleCompiler: failed to compile index expression for index "
              "access.");

    bigint min = 0;
    bigint max = type.numBytes() - 1;
    solAssert(max > 0, "IeleCompiler: found bytes0 type");
    appendRangeCheck(IndexValue, &min, &max);

    iele::IeleLocalVariable *ByteValue =
      iele::IeleLocalVariable::Create(&Context, "tmp",
                                      CompilingFunction);
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Sub, ByteValue,
      iele::IeleIntConstant::Create(&Context, bigint(max)),
      IndexValue, CompilingBlock);
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Byte, ByteValue, ByteValue, ExprValue, CompilingBlock);
    CompilingExpressionResult.push_back(ByteValue);
    break;
  }
  case Type::Category::Mapping: {
    const MappingType &type = dynamic_cast<const MappingType &>(baseType);
    TypePointer keyType = type.keyType();

    // Visit accessed exression.
    iele::IeleValue *ExprValue = compileExpression(indexAccess.baseExpression());
    solAssert(ExprValue,
              "IeleCompiler: failed to compile base expression for index "
              "access.");

    // Visit index expression.
    solAssert(indexAccess.indexExpression(),
              "IeleCompiler: Index expression expected.");
    iele::IeleValue *IndexValue =
      compileExpression(*indexAccess.indexExpression());
    IndexValue =
      appendTypeConversion(
          IndexValue,
          indexAccess.indexExpression()->annotation().type, keyType);
    IndexValue = encoding(IndexValue, keyType);
    solAssert(IndexValue,
              "IeleCompiler: failed to compile index expression for index "
              "access.");
    IeleLValue *AddressValue = appendMappingAccess(type, IndexValue, ExprValue);

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
  case DataLocation::Memory: {
    elementSize = elementType->memorySize();
    size = type.memorySize();
    break;
  }
  case DataLocation::CallData:
    solAssert(false, "not supported by IELE.");
  }
  // First compute the offset from the start of the array.
  iele::IeleValue *ElementSizeValue =
    iele::IeleIntConstant::Create(&Context, elementSize);
  iele::IeleLocalVariable *OffsetValue =
    iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
  if (!type.isByteArray()) {
    iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Mul, OffsetValue, IndexValue,
        ElementSizeValue, CompilingBlock);
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
    SizeValue = iele::IeleIntConstant::Create(&Context, size);
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
                  dev::bitsRequired(NextStorageAddress));
    iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Add, AddressValue, ExprValue, AddressValue,
        CompilingBlock);
  } else {
    // In this case AddressValue = ExprValue + IndexValue * ValueTypeSize
    iele::IeleValue *ValueTypeSize =
      iele::IeleIntConstant::Create(&Context, valueType->storageSize());
    iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Mul, AddressValue, IndexValue,
        ValueTypeSize, CompilingBlock);
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
  case DataLocation::Memory: {
    offset = type.memoryOffsetOfMember(member);
    break;
  }
  default:
    solAssert(false, "IeleCompiler: Illegal data location for struct.");
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

void IeleCompiler::appendRangeCheck(iele::IeleValue *Value, const Type &Type) {
  bigint min, max;
  switch(Type.category()) {
  case Type::Category::Integer: {
    const IntegerType &type = dynamic_cast<const IntegerType &>(Type);
    if (type.isUnbound() && type.isSigned()) {
      return;
    } else if (type.isUnbound()) {
      bigint min = 0;
      if (iele::IeleIntConstant *constant = llvm::dyn_cast<iele::IeleIntConstant>(Value)) {
        if (constant->getValue() < 0) {
          appendRevert();
        }
      } else {
        appendRangeCheck(Value, &min, nullptr);
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
  default:
    solAssert(false, "not implemented yet");
  }
  appendRangeCheck(Value, &min, &max);
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
    CompilingExpressionResult.push_back(RegisterLValue::Create(llvm::dyn_cast<iele::IeleLocalVariable>(Identifier)));
    return;
  }

  // If not found, lookup identifier in the contract's symbol table.
  bool isValueType = true;
  if (const VariableDeclaration *variableDecl =
        dynamic_cast<const VariableDeclaration *>(declaration)) {
    isValueType = variableDecl->annotation().type->isValueType();
    if (variableDecl->isConstant()) {
      iele::IeleValue *Identifier = compileExpression(*variableDecl->value());
      TypePointer rhsType = variableDecl->value()->annotation().type;
      Identifier = appendTypeConversion(Identifier, rhsType, variableDecl->annotation().type);
      CompilingExpressionResult.push_back(Identifier);
      return;
    }
  }
  ST = CompilingContract->getIeleValueSymbolTable();
  solAssert(ST,
            "IeleCompiler: failed to access compiling contract's symbol "
            "table.");
  if (iele::IeleValue *Identifier = ST->lookup(name)) {
    appendVariable(Identifier, identifier.name(), isValueType);
    return;
  }

  // If not found, make a new IeleLocalVariable for the identifier.
  iele::IeleLocalVariable *Identifier =
    iele::IeleLocalVariable::Create(&Context, name, CompilingFunction);
  CompilingExpressionResult.push_back(RegisterLValue::Create(Identifier));
  return;
}

void IeleCompiler::appendVariable(iele::IeleValue *Identifier, std::string name,
                                  bool isValueType) {
    if (llvm::isa<iele::IeleGlobalVariable>(Identifier)) {
      if (isValueType) {
        // In case of a global variable, if we aren't compiling an lvalue and
        // the global variable holds a value type, we have to load the global
        // variable.
        CompilingExpressionResult.push_back(AddressLValue::Create(this, Identifier, DataLocation::Storage, name));
      } else {
        CompilingExpressionResult.push_back(ReadOnlyLValue::Create(Identifier));
      }
    } else if (auto Local = llvm::dyn_cast<iele::IeleLocalVariable>(Identifier)) {
      CompilingExpressionResult.push_back(RegisterLValue::Create(Local));
    } else {
      CompilingExpressionResult.push_back(ReadOnlyLValue::Create(Identifier));
    }
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
  case Type::Category::StringLiteral: {
    const auto &literalType = dynamic_cast<const StringLiteralType &>(*type);
    std::string value = literalType.value();
    std::reverse(value.begin(), value.end());
    bigint value_integer = bigint(toHex(asBytes(value), 2, HexPrefix::Add));
    iele::IeleIntConstant *LiteralValue =
      iele::IeleIntConstant::Create(
        &Context,
        value_integer,
        true);
    CompilingExpressionResult.push_back(LiteralValue);
    break;
  }
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
  solAssert(CompilingExpressionResult.size() == 1,
            "IeleCompiler: Expression visitor did not set enough result values");
  Result = CompilingExpressionResult[0].rval(CompilingBlock);

  // Restore expression compilation status and return result.
  CompilingExpressionResult.clear();
  CompilingLValue = SavedCompilingLValue;
  return Result;
}

void IeleCompiler::compileExpressions(
  const Expression &expression, 
  llvm::SmallVectorImpl<iele::IeleValue *> &Result,
  unsigned numExpressions) {
  // Save current expression compilation status.
  bool SavedCompilingLValue = CompilingLValue;

  // Visit expression.
  CompilingLValue = false;
  expression.accept(*this);

  // Expression visitors should store the value that is the result of the
  // compiled for the expression computation in the CompilingExpressionResult
  // field. This helper should only be used when a scalar value (or void) is
  // expected as the result of the corresponding expression computation.
  solAssert(CompilingExpressionResult.size() == numExpressions, "expression visitor did not set enough result values");
  for (Value val : CompilingExpressionResult) {
    Result.push_back(val.rval(CompilingBlock));
  }

  // Restore expression compilation status and return result.
  CompilingExpressionResult.clear();
  CompilingLValue = SavedCompilingLValue;
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
      llvm::SmallVector<iele::IeleLocalVariable *, 0> Returns;
      llvm::SmallVector<iele::IeleValue *, 4> Arguments;

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
            compileFunctionArguments(&Arguments, &Returns, *arguments, 
                                    function, false);
            ModifierDepth = ModifierDepthCache;
            solAssert(Returns.size() == 0, "Constructor doesn't return anything");
          }
        } else {
          // forward aux param to base constructor
          for (std::string auxParamName : forwardingAuxParams.first) {
            iele::IeleValue *AuxArg =
              FunST->lookup(VarNameMap[NumOfModifiers][auxParamName]);
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
                  FunST->lookup(VarNameMap[NumOfModifiers][pName]);
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
                const FunctionType &function = 
                  FunctionType(*auxParamDest->constructor());
                llvm::SmallVector<iele::IeleValue *, 4> AuxArguments;

                // Cache ModifierDepth
                unsigned ModifierDepthCache = ModifierDepth;
                ModifierDepth = NumOfModifiers;

                // compile args 
                for (unsigned i = 0; i < arguments->size(); ++i) {
                  iele::IeleValue *ArgValue = compileExpression(*(*arguments)[i]);
                  solAssert(ArgValue,
                            "IeleCompiler: Failed to compile internal function call "
                            "argument");
                  AuxArguments.push_back(ArgValue);
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
      iele::IeleInstruction::CreateInternalCall(Returns, CalleeValue, Arguments, CompilingBlock);
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
    iele::IeleValue *InitValue = nullptr;
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
    IeleLValue *LHSValue = makeLValue(ST->lookup(getIeleNameForStateVariable(stateVariable)), type, DataLocation::Storage);
    solAssert(LHSValue, "IeleCompiler: Failed to compile LHS of state variable"
                        " initialization");

    if (type->isValueType()) {
      // Add assignment in constructor's body.
      LHSValue->write(InitValue, CompilingBlock);
    } else if (type->category() == Type::Category::Array ||
               type->category() == Type::Category::Struct) {
      // Add code for copy in constructor's body.
      rhsType = rhsType->mobileType();
      if (shouldCopyStorageToStorage(LHSValue, *rhsType))
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
    iele::IeleLocalVariable *Local, const VariableDeclaration *localVariable) {
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
    solAssert(type->isValueType() ||
              type->category() == Type::Category::Mapping,
              "IeleCompiler: found local variable of unknown type");
    // Local variables are always automatically initialized to zero. We don't
    // need to do anything here.
  }

  // Add assignment if needed.
  if (InitValue)
    iele::IeleInstruction::CreateAssign(Local, InitValue, CompilingBlock);
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
      case DataLocation::Memory: {
        elementSize = elementType.memorySize();
        break;
      }
      case DataLocation::CallData:
        solAssert(false, "not supported by IELE.");
      }
 
      iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Mul, SizeVariable, SizeValue, 
        iele::IeleIntConstant::Create(&Context, elementSize),
        CompilingBlock);
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
    case DataLocation::Memory: {
      elementSize = elementType.memorySize();
      break;
    }
    case DataLocation::CallData:
      solAssert(false, "not supported by IELE.");
    }

    SizeValue =
      iele::IeleLocalVariable::Create(&Context, "tmp", CompilingFunction);
    iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Mul,
        llvm::cast<iele::IeleLocalVariable>(SizeValue),
        iele::IeleIntConstant::Create(&Context, elementSize),
        NumElemsValue, CompilingBlock);
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

void IeleCompiler::appendCopy(
    IeleLValue *To, TypePointer ToType,
    iele::IeleValue *From, TypePointer FromType,
    DataLocation ToLoc, DataLocation FromLoc) {
  iele::IeleValue *AllocedValue;
  if (FromType->isValueType()) {
    solAssert(ToType->isValueType(), "invalid conversion");
    AllocedValue = From;
    AllocedValue = appendTypeConversion(AllocedValue, FromType, ToType);
    To->write(AllocedValue, CompilingBlock);
    return;
  }

  if (!FromType->isDynamicallyEncoded() && *FromType == *ToType) {
    iele::IeleValue *SizeValue = getReferenceTypeSize(*FromType, From);
    if (dynamic_cast<RegisterLValue *>(To)) {
      AllocedValue = appendIeleRuntimeAllocateMemory(SizeValue);
      To->write(AllocedValue, CompilingBlock);
    } else {
      AllocedValue = To->read(CompilingBlock);
    }
    appendIeleRuntimeCopy(From, AllocedValue, SizeValue, FromLoc, ToLoc);
    return;
  }
  switch (FromType->category()) {
  case Type::Category::Struct: {
    const StructType &structType = dynamic_cast<const StructType &>(*FromType);
    const StructType &toStructType = dynamic_cast<const StructType &>(*ToType);

    if (dynamic_cast<RegisterLValue *>(To)) {
      AllocedValue = appendStructAllocation(toStructType);
      To->write(AllocedValue, CompilingBlock);
    } else {
      AllocedValue = To->read(CompilingBlock);
    }
    // We loop over the members of the source struct type.
    for (auto const &member : structType.members(nullptr)) {
      // First find the types of the corresponding member in the source and
      // destination struct types.
      TypePointer memberFromType = member.type;
      TypePointer memberToType;
      for (auto const &toMember : toStructType.members(nullptr)) {
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

      // Then compute the offset from the start of the struct for the current
      // member. The offset may differ in the source and destination, due to
      // skipping of mappings in memory copies of structs.
      bigint fromOffset, toOffset;
      switch (FromLoc) {
      case DataLocation::Storage: {
        const auto& offsets = structType.storageOffsetsOfMember(member.name);
        fromOffset = offsets.first;
        break;
      }
      case DataLocation::Memory: {
        fromOffset = structType.memoryOffsetOfMember(member.name);
        break;
      case DataLocation::CallData:
        solAssert(false, "Call data not supported in IELE");
      }
      }
      switch (ToLoc) {
      case DataLocation::Storage: {
        const auto& offsets = toStructType.storageOffsetsOfMember(member.name);
        toOffset = offsets.first;
        break;
      }
      case DataLocation::Memory: {
        toOffset = toStructType.memoryOffsetOfMember(member.name);
        break;
      case DataLocation::CallData: 
        solAssert(false, "Call data not supported in IELE");
      }
      }

      // Then compute the current member address in the source and destination
      // struct objects.
      iele::IeleLocalVariable *MemberFrom =
        iele::IeleLocalVariable::Create(&Context, "copy.from.address", CompilingFunction);
      iele::IeleValue *FromOffsetValue =
        iele::IeleIntConstant::Create(&Context, fromOffset);
      iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Add, MemberFrom, From, FromOffsetValue,
        CompilingBlock);

      iele::IeleLocalVariable *MemberTo =
        iele::IeleLocalVariable::Create(&Context, "copy.to.address", CompilingFunction);
      iele::IeleValue *ToOffsetValue =
        iele::IeleIntConstant::Create(&Context, toOffset);
      iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Add, MemberTo, AllocedValue, ToOffsetValue,
        CompilingBlock);

      // Finally do the copy of the member value from source to destination.
      appendCopy(makeLValue(MemberTo, memberToType, ToLoc), memberToType, makeLValue(MemberFrom, memberFromType, FromLoc)->read(CompilingBlock), memberFromType, ToLoc, FromLoc);
    }
    break;
  }
  case Type::Category::Array: {
    const ArrayType &arrayType = dynamic_cast<const ArrayType &>(*FromType);
    const ArrayType &toArrayType = dynamic_cast<const ArrayType &>(*ToType);
    TypePointer elementType = arrayType.baseType();
    TypePointer toElementType = toArrayType.baseType();
    if (arrayType.isByteArray() && toArrayType.isByteArray()) {
      // copy from bytes to string or back
      iele::IeleValue *SizeValue = getReferenceTypeSize(*FromType, From);
      if (dynamic_cast<RegisterLValue *>(To)) {
        AllocedValue = appendIeleRuntimeAllocateMemory(SizeValue);
        To->write(AllocedValue, CompilingBlock);
      } else {
        AllocedValue = To->read(CompilingBlock);
      }
      appendIeleRuntimeCopy(From, AllocedValue, SizeValue, FromLoc, ToLoc);
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
          SizeVariableFrom, From,
          CompilingBlock) :
        iele::IeleInstruction::CreateLoad(
          SizeVariableFrom, From,
          CompilingBlock) ;
      iele::IeleInstruction::CreateBinOp(
          iele::IeleInstruction::Add, ElementFrom, From,
          iele::IeleIntConstant::getOne(&Context),
          CompilingBlock);
    } else {
      iele::IeleInstruction::CreateAssign(
        SizeVariableFrom, iele::IeleIntConstant::Create(&Context, bigint(arrayType.length())),
        CompilingBlock);
      iele::IeleInstruction::CreateAssign(
        ElementFrom, From, CompilingBlock);
    }

    // copy the size field
    if (ToType->isDynamicallySized()) {
      if (ToLoc == DataLocation::Storage) {
        AllocedValue = To->read(CompilingBlock);
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
        To->write(AllocedValue, CompilingBlock);
      }

      iele::IeleInstruction::CreateBinOp(
          iele::IeleInstruction::Add, ElementTo, AllocedValue,
          iele::IeleIntConstant::getOne(&Context),
          CompilingBlock);
    } else {
      if (dynamic_cast<RegisterLValue *>(To)) {
        AllocedValue = appendArrayAllocation(toArrayType);
        To->write(AllocedValue, CompilingBlock);
      } else {
        AllocedValue = To->read(CompilingBlock);
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
    case DataLocation::Memory: {
      elementSize = elementType->memorySize();
      break;
    }
    case DataLocation::CallData:
      solAssert(false, "not supported by IELE.");
    }

    switch (ToLoc) {
    case DataLocation::Storage: {
      toElementSize = toElementType->storageSize();
      break;
    }
    case DataLocation::Memory: {
      toElementSize = toElementType->memorySize();
      break;
    }
    case DataLocation::CallData:
      solAssert(false, "not supported by IELE.");
    }

    iele::IeleValue *FromElementSizeValue =
        iele::IeleIntConstant::Create(&Context, elementSize);
    iele::IeleValue *ToElementSizeValue =
        iele::IeleIntConstant::Create(&Context, toElementSize);

    iele::IeleLocalVariable *FillLoc, *FillSize;
    if (!elementType->isDynamicallyEncoded() && *elementType == *toElementType) {
      iele::IeleLocalVariable *CopySize =
        iele::IeleLocalVariable::Create(&Context, "copy.size", CompilingFunction);
      iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Mul, CopySize, SizeVariableFrom, FromElementSizeValue,
        CompilingBlock);
      appendIeleRuntimeCopy(ElementFrom, ElementTo, CopySize, FromLoc, ToLoc);

      FillLoc = iele::IeleLocalVariable::Create(&Context, "fill.address", CompilingFunction);
      iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Add, FillLoc, ElementTo, CopySize,
        CompilingBlock);

      FillSize = iele::IeleLocalVariable::Create(&Context, "fill.size", CompilingFunction);
      iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Mul, FillSize, SizeVariableTo, ToElementSizeValue,
        CompilingBlock);
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
      iele::IeleInstruction::CreateBinOp(
        iele::IeleInstruction::Mul, SizeVariableTo, SizeVariableTo, ToElementSizeValue);
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

void IeleCompiler::appendCopyFromStorageToStorage(
    IeleLValue *To, TypePointer ToType, iele::IeleValue *From, TypePointer FromType) {
  appendCopy(To, ToType, From, FromType, DataLocation::Storage, DataLocation::Storage);
}
void IeleCompiler::appendCopyFromMemoryToStorage(
    IeleLValue *To, TypePointer ToType, iele::IeleValue *From, TypePointer FromType) {
  appendCopy(To, ToType, From, FromType, DataLocation::Storage, DataLocation::Memory);
}
void IeleCompiler::appendCopyFromMemoryToMemory(
    IeleLValue *To, TypePointer ToType, iele::IeleValue *From, TypePointer FromType) {
  appendCopy(To, ToType, From, FromType, DataLocation::Memory, DataLocation::Memory);
}

iele::IeleValue *IeleCompiler::appendCopyFromStorageToMemory(
    TypePointer ToType, iele::IeleValue *From, TypePointer FromType) {
  iele::IeleLocalVariable *To =
    iele::IeleLocalVariable::Create(&Context, "copy.to", CompilingFunction);
  appendCopy(RegisterLValue::Create(To), ToType, From, FromType, DataLocation::Memory, DataLocation::Storage);
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
    iele::IeleIntConstant *ElementSize =
      iele::IeleIntConstant::Create(&Context, elementSize);
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Mul, NewEnd, NewLength, 
      ElementSize,
      CompilingBlock);
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Add, NewEnd, NewEnd,
      LValue, CompilingBlock);
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Add, NewEnd, NewEnd,
      iele::IeleIntConstant::getOne(&Context),
      CompilingBlock);
    iele::IeleLocalVariable *DeleteSize =
      iele::IeleLocalVariable::Create(&Context, "size.to.delete", CompilingFunction);
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Sub, DeleteSize, OldLength, NewLength,
      CompilingBlock);
    iele::IeleInstruction::CreateBinOp(
      iele::IeleInstruction::Mul, DeleteSize, DeleteSize, 
      ElementSize, CompilingBlock); 
    appendIeleRuntimeFill(
      NewEnd, DeleteSize,
      iele::IeleIntConstant::getZero(&Context),
      DataLocation::Storage);
  }

  JoinBlock->insertInto(CompilingFunction);
  CompilingBlock = JoinBlock;
}

iele::IeleLocalVariable *IeleCompiler::appendBooleanOperator(
    Token::Value Opcode,
    const Expression &LeftOperand,
    const Expression &RightOperand) {
  solAssert(Opcode == Token::Or || Opcode == Token::And, "IeleCompiler: invalid boolean operator");

  iele::IeleValue *LeftOperandValue = 
    compileExpression(LeftOperand);
  solAssert(LeftOperandValue, "IeleCompiler: Failed to compile left operand.");
  llvm::SmallVector<iele::IeleLocalVariable *, 1> Results;
  if (Opcode == Token::Or) {
    appendConditional(
      LeftOperandValue, Results,
      [this](llvm::SmallVectorImpl<iele::IeleValue *> &Results){ Results.push_back(iele::IeleIntConstant::getOne(&Context)); },
      [&RightOperand, this](llvm::SmallVectorImpl<iele::IeleValue *> &Results){Results.push_back(compileExpression(RightOperand));});
    solAssert(Results.size() == 1, "Boolean operators have a single results");
    return Results[0];
  } else {
    appendConditional(
      LeftOperandValue, Results,
      [&RightOperand, this](llvm::SmallVectorImpl<iele::IeleValue *> &Results){Results.push_back(compileExpression(RightOperand));},
      [this](llvm::SmallVectorImpl<iele::IeleValue *> &Results){Results.push_back(iele::IeleIntConstant::getZero(&Context)); });
    solAssert(Results.size() == 1, "Boolean operators have a single results");
    return Results[0];
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
    fixed = !type->isUnbound();
    nbytes = fixed ? type->numBits() / 8 : 0;
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
    if (fixed) {
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
          iele::IeleIntConstant::Create(&Context, bigint(nbytes-1));
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
    if (fixed) {
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
          iele::IeleIntConstant::Create(&Context, bigint(nbytes-1));
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
    // fall through
  }
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

 void IeleCompiler::appendTypeConversions(
  llvm::SmallVectorImpl<iele::IeleValue *> &Results, 
  llvm::SmallVectorImpl<iele::IeleValue *> &RHSValues,
  TypePointer SourceType, TypePointers TargetTypes) {

  TypePointers RHSTypes;
  if (const TupleType *tupleType =
        dynamic_cast<const TupleType *>(SourceType.get())) {
    RHSTypes = tupleType->components();
  } else {
    RHSTypes = TypePointers{SourceType};
  }

  solAssert(RHSValues.size() == RHSTypes.size(),
            "IeleCompiler: Missing value in tuple in conditional expression");

  solAssert(TargetTypes.size() == RHSTypes.size(),
            "IeleCompiler: Missing value in tuple in conditional expression");

  int i = 0;
  for (iele::IeleValue *RHSValue : RHSValues) {
    TypePointer LHSType = TargetTypes[i];
    TypePointer RHSType = RHSTypes[i];
    iele::IeleValue *Result = appendTypeConversion(RHSValue, RHSType, LHSType);
    Results.push_back(Result);
    i++;
  }
}


iele::IeleValue *IeleCompiler::appendTypeConversion(iele::IeleValue *Value, TypePointer SourceType, TypePointer TargetType) {
  if (*SourceType == *TargetType) {
    return Value;
  }
  iele::IeleLocalVariable *convertedValue =
    iele::IeleLocalVariable::Create(&Context, "type.converted", CompilingFunction);
  switch (SourceType->category()) {
  case Type::Category::FixedBytes: {
    const FixedBytesType &srcType = dynamic_cast<const FixedBytesType &>(*SourceType);
    if (TargetType->category() == Type::Category::Integer) {
      IntegerType const& targetType = dynamic_cast<const IntegerType &>(*TargetType);
      if (!targetType.isUnbound() && targetType.numBits() < srcType.numBytes() * 8) {
        appendMask(convertedValue, Value, targetType.numBits() / 8, targetType.isSigned());
        return convertedValue;
      }
      return Value;
    } else {
      solAssert(TargetType->category() == Type::Category::FixedBytes, "Invalid type conversion requested.");
      const FixedBytesType &targetType = dynamic_cast<const FixedBytesType &>(*TargetType);
      appendShiftBy(convertedValue, Value, 8 * (targetType.numBytes() - srcType.numBytes()));
      return convertedValue;
    }
  }
  case Type::Category::Enum:
    solAssert(*SourceType == *TargetType || TargetType->category() == Type::Category::Integer, "Invalid enum conversion");
    return Value;
  case Type::Category::FixedPoint:
  case Type::Category::Function: {
    solAssert(false, "not implemented yet");
    break;
  case Type::Category::Struct:
  case Type::Category::Array:
    if (shouldCopyStorageToMemory(*TargetType, *SourceType))
      return appendCopyFromStorageToMemory(TargetType, Value, SourceType);
    return Value;
  case Type::Category::Integer:
  case Type::Category::RationalNumber:
  case Type::Category::Contract: {
    switch(TargetType->category()) {
    case Type::Category::FixedBytes: {
      solAssert(SourceType->category() == Type::Category::Integer || SourceType->category() == Type::Category::RationalNumber,
        "Invalid conversion to FixedBytesType requested.");
      if (auto srcType = dynamic_cast<const IntegerType *>(&*SourceType)) {
        const FixedBytesType &targetType = dynamic_cast<const FixedBytesType &>(*TargetType);
        if (srcType->isUnbound() || targetType.numBytes() * 8 < srcType->numBits()) {
          appendMask(convertedValue, Value, targetType.numBytes(), false);
          return convertedValue;
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
    case Type::Category::Contract: {
      IntegerType const& targetType = TargetType->category() == Type::Category::Integer
        ? dynamic_cast<const IntegerType &>(*TargetType) : *Address;
      if (SourceType->category() == Type::Category::RationalNumber) {
        solAssert(!dynamic_cast<const RationalNumberType &>(*SourceType).isFractional(), "not implemented yet");
      }
      if (targetType.isUnbound()) {
        if (!targetType.isSigned()) {
          if (iele::IeleIntConstant *constant = llvm::dyn_cast<iele::IeleIntConstant>(Value)) {
            if (constant->getValue() < 0) {
              appendRevert();
            }
          } else if (auto srcType = dynamic_cast<const IntegerType *>(&*SourceType)) {
            if (srcType->isSigned()) {
              bigint min = 0;
              appendRangeCheck(Value, &min, nullptr);
            }
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
      return Result;
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
        Value, ValueAddress, CompilingBlock);
      return Ptr;
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
    NBytesValue =
      iele::IeleIntConstant::Create(&Context, bigint(nbytes-1));
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

bool IeleCompiler::shouldCopyStorageToStorage(const IeleLValue *To,
                                              const Type &From) const {
  return dynamic_cast<const ReadOnlyLValue *>(To) &&
         From.dataStoredIn(DataLocation::Storage);
}

bool IeleCompiler::shouldCopyMemoryToStorage(const Type &ToType, const IeleLValue *To,
                                             const Type &FromType) const {
  return dynamic_cast<const ReadOnlyLValue *>(To) &&
         FromType.dataStoredIn(DataLocation::Memory) &&
         ToType.dataStoredIn(DataLocation::Storage);
}

bool IeleCompiler::shouldCopyMemoryToMemory(const Type &ToType, const IeleLValue *To,
                                             const Type &FromType) const {
  return dynamic_cast<const ReadOnlyLValue *>(To) &&
         FromType.dataStoredIn(DataLocation::Memory) &&
         ToType.dataStoredIn(DataLocation::Memory);
}

bool IeleCompiler::shouldCopyStorageToMemory(const Type &ToType,
                                             const Type &FromType) const {
  return ToType.dataStoredIn(DataLocation::Memory) &&
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
  case DataLocation::Memory:
    name = "ielert.memory.fill";
    break;
  case DataLocation::CallData:
    solAssert(false, "not implemented in IELE");
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
