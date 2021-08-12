#pragma once

#include "libiele/IeleContext.h"
#include "libiele/IeleContract.h"
#include "libsolidity/ast/ASTVisitor.h"
#include "libevmasm/LinkerObject.h"

#include "libsolidity/codegen/IeleRValue.h"

#include "llvm/Support/raw_ostream.h"

#include <map>

namespace solidity {
namespace iele {

class IeleContract;

} // end namespace iele

namespace frontend {

class IeleLValue;
class IeleRValue;

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
    ECRFailedBlock(nullptr),
    AssertFailBlock(nullptr),
    CompilingBlockArithmetic(Arithmetic::Checked),
    CompilingLValue(false),
    NextStorageAddress(1),
    CompilingContractASTNode(nullptr),
    CompilingFunctionASTNode(nullptr),
    ModifierDepth(-1) { }

  // Compiles a contract.
  void compileContract(
      const ContractDefinition &contract,
      const std::map<const ContractDefinition *, std::shared_ptr<IeleCompiler const>> &otherCompilers,
      const bytes &metadata);

  // Returns the compiled IeleContract.
  iele::IeleContract &assembly() const {
    solAssert(CompiledContract,
              "Attempted access to compiled contract before compiling successfully.");
    return *CompiledContract;
  }

  std::string assemblyString(const StringMap &_sourceCodes = StringMap()) const {
    std::string ret;
    llvm::raw_string_ostream OS(ret);
    CompiledContract->print(OS);
    return ret;
  }

  Json::Value assemblyJSON(const StringMap &_sourceCodes = StringMap()) const {
    Json::Value value;
    value["code"] = assemblyString(_sourceCodes);
    return value;
  }

  evmasm::LinkerObject assembledObject() const {
    bytes bytecode = CompiledContract->toBinary();
    return {bytecode,
            std::map<size_t, std::string>(),
            std::map<u256, std::pair<std::string, std::vector<size_t>>>()};
  }

  // Update currently enabled set of experimental features.
  void setExperimentalFeatures(const std::set<ExperimentalFeature> &features) {
    ExperimentalFeatures = features;
  }

  // Returns true if the given feature is enabled.
  bool experimentalFeatureActive(ExperimentalFeature feature) const {
    return ExperimentalFeatures.count(feature);
  }

  // Visitor interface.
  virtual bool visit(const FunctionDefinition &function) override;
  virtual bool visit(const Block &block) override;
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
  virtual bool visit(const FunctionCallOptions &functionCallOptions) override;
  virtual bool visit(const NewExpression &newExpression) override;
  virtual bool visit(const MemberAccess &memberAccess) override;
  virtual bool visit(const IndexAccess &indexAccess) override;
  virtual bool visit(const IndexRangeAccess &indexRangeAccess) override;
  virtual bool visit(const ElementaryTypeNameExpression &typeName) override;
  virtual void endVisit(const Block &block) override;
  virtual void endVisit(const Identifier &identifier) override;
  virtual void endVisit(const Literal &literal) override;

  friend class AddressLValue;
  friend class ByteArrayLValue;
  friend class ArrayLengthLValue;
  friend class RecursiveStructLValue;

private:
  iele::IeleContract *CompiledContract;
  std::map<const ContractDefinition *, std::shared_ptr<IeleCompiler const>> OtherCompilers;
  iele::IeleContext Context;
  iele::IeleContract *CompilingContract;
  std::vector<const FunctionDefinition *> CompilingContractFreeFunctions;
  iele::IeleFunction *CompilingFunction;
  iele::IeleLocalVariable *CompilingFunctionStatus;
  iele::IeleBlock *CompilingBlock;
  iele::IeleBlock *BreakBlock;
  iele::IeleBlock *ContinueBlock;
  iele::IeleBlock *RevertBlock;
  iele::IeleBlock *RevertStatusBlock;
  iele::IeleBlock *ECRFailedBlock;
  iele::IeleBlock *AssertFailBlock;

  Arithmetic CompilingBlockArithmetic;
  void setArithmetic(Arithmetic arithmetic) {
    CompilingBlockArithmetic = arithmetic;
  }
  Arithmetic getArithmetic() const { return CompilingBlockArithmetic; }

  struct Value {
  private:
    bool isLValue;
    union {
      IeleRValue *RValue;
      IeleLValue *LValue;
    };
  public:
    Value(IeleRValue *RValue) : isLValue(false), RValue(RValue) {}
    Value(IeleLValue *LValue) : isLValue(true), LValue(LValue) {}
    IeleRValue *rval(iele::IeleBlock *Block);
    IeleLValue *lval() { solAssert(isLValue, "IeleCompiler: expression is not an lvalue"); return LValue; }
  };

  llvm::SmallVector<Value, 4> CompilingExpressionResult;
  bool CompilingLValue;
  const ArrayType *CompilingLValueArrayType;

  bigint NextStorageAddress;

  std::vector<const ContractDefinition *> CompilingContractInheritanceHierarchy;
  const ContractDefinition *CompilingContractASTNode;
  const FunctionDefinition *CompilingFunctionASTNode;

  std::string getIeleNameForContract(const ContractDefinition *function);
  std::string getIeleNameForFunction(const FunctionDefinition &function);
  std::string getIeleNameForStateVariable(const VariableDeclaration *stateVariable);
  std::string getIeleNameForAccessor(const VariableDeclaration *stateVariable);
  std::string getIeleNameForLocalVariable(const VariableDeclaration *localVariable);

  std::set<ExperimentalFeature> ExperimentalFeatures;

  // Fills in the ctorAuxParams data structure i.e. for each constructor in the 
  // class hierarchy, it computes the needed extra parameters and the additional
  // information needed to correctly handle them. 
  void computeCtorsAuxParams();
  
  // Data structure to hold auxiliary constructor params
  // Ctor -> Dest -> (paramName, Source)
  std::map<const ContractDefinition *, 
           std::map<const ContractDefinition *, 
                    std::pair<std::vector<std::string>, 
                              const ContractDefinition *>>> ctorAuxParams;  

  // Infrastructure for handling modifiers (borrowed from ContractCompiler.cpp)
  // Lookup function modifier by name
  const ModifierDefinition *functionModifier(const std::string &_name) const;
  void appendReturn(const FunctionDefinition &function, 
                    llvm::SmallVector<TypePointer, 4> ReturnParameterTypes); 
  // Appends one layer of function modifier code of the current function, or the
  // function body itself if the last modifier was reached.
  void appendModifierOrFunctionCode();
  unsigned ModifierDepth;
  // Maps each level of modifiers with a return target
  std::stack<iele::IeleBlock *> ReturnBlocks;  
  // Return parameters of the current function 
  // TODO: do we really want to have this here?
  std::vector<IeleLValue *> CompilingFunctionReturnParameters;
  
  template <typename ExpressionClass>
  void compileFunctionArguments(
      llvm::SmallVectorImpl<iele::IeleValue *> &Arguments,
      llvm::SmallVectorImpl<iele::IeleLocalVariable *> &ReturnRegisters, 
      llvm::SmallVectorImpl<IeleLValue *> &Returns,
      const std::vector<ASTPointer<ExpressionClass>> &arguments,
      const FunctionType &function, bool encode);

  // Infrastructure for unique variable names generation and mapping
  int NextUniqueIntToken = 0;
  int getNextUniqueIntToken();
  std::string getNextVarSuffix();
  // It is not enough to map names to names; we need such a mapping for each "layer",
  // that is for each function modifier.
  std::map<unsigned, std::map <std::string, IeleLValue *>> VarNameMap;

  // Helpers for the compilation process.
  IeleRValue *compileExpression(const Expression &expression);
  void compileTuple(
      const Expression &expression,
      llvm::SmallVectorImpl<IeleRValue *> &Result);
  IeleLValue *compileLValue(const Expression &expression);
  void compileLValues(
      const Expression &expression,
      llvm::SmallVectorImpl<IeleLValue *> &Result);

  void connectWithUnconditionalJump(iele::IeleBlock *SourceBlock, 
                                    iele::IeleBlock *DestinationBlock);
  void connectWithConditionalJump(iele::IeleValue *Condition,
                                  iele::IeleBlock *SouceBlock, 
                                  iele::IeleBlock *DestinationBlock);

  iele::IeleValue *convertFunctionToInternal(iele::IeleValue *Callee);

  void appendPayableCheck(void);
  void appendRevert(iele::IeleValue *Condition = nullptr, iele::IeleValue *Status = nullptr);
  void appendInvalid(iele::IeleValue *Condition = nullptr);
  void appendRangeCheck(iele::IeleValue *Value, bigint *min, bigint *max);
  void appendRangeCheck(IeleRValue *Value, const Type &Type);
  void appendGasLeft(void);

  void appendRevertBlocks(void);

  void appendDefaultConstructor(const ContractDefinition *contract);
  void appendLocalVariableInitialization(
          IeleLValue *Local, const VariableDeclaration *localVariable);

  IeleLValue *appendMappingAccess(const MappingType &type, iele::IeleValue *IndexValue, iele::IeleValue *ExprValue);
  void appendArrayAccessRangeCheck(const ArrayType &type, iele::IeleValue *IndexValue, iele::IeleValue *ExprValue, DataLocation Loc, bigint sizeInElements);
  IeleLValue *appendArrayAccess(const ArrayType &type, iele::IeleValue *IndexValue, iele::IeleValue *ExprValue, DataLocation Loc);
  IeleRValue *appendArrayRangeAccess(const ArrayType &type, iele::IeleValue *StartValue, iele::IeleValue *EndValue, iele::IeleValue *ExprValue, DataLocation Loc);
  IeleLValue *appendStructAccess(const StructType &type, iele::IeleValue *ExprValue, std::string member, DataLocation Loc);

  void appendMul(iele::IeleLocalVariable *LValue, iele::IeleValue *LeftOperand, bigint RightOperand);

  iele::IeleValue *getReferenceTypeSize(
    const Type &type, iele::IeleValue *AddressValue);

  // Helper function for memory array allocation. The optional NumElemsValue
  // argument can be used to allocate dynamically-sized arrays, where the initial
  // size is not known at compile time.
  iele::IeleLocalVariable *appendArrayAllocation(
      const ArrayType &type, iele::IeleValue *NumElemsValue = nullptr);

  // Helper function for memory struct allocation.
  iele::IeleLocalVariable *appendStructAllocation(const StructType &type);

  // Helper function for a recursive struct allocation in storage.
  iele::IeleLocalVariable *appendRecursiveStructAllocation(
      const StructType &type);

  // Helper functions for copying a reference type to a storage location.
  void appendCopyFromStorageToStorage(
      IeleLValue *To, TypePointer ToType, IeleRValue *From, TypePointer FromType);
  void appendCopyFromMemoryToStorage(
      IeleLValue *To, TypePointer ToType, IeleRValue *From, TypePointer FromType);

  void appendCopyFromMemoryToMemory(
      IeleLValue *To, TypePointer ToType, IeleRValue *From, TypePointer FromType);

  // Helper function for copying a storage reference type into a local newly
  // allocated memory copy. Returns a pointer to the copy.
  iele::IeleValue *appendCopyFromStorageToMemory(
    TypePointer ToType, IeleRValue *From, TypePointer FromType);

  void appendArrayCopyLoop(
      IeleLValue *To, const ArrayType &toArrayType,
      IeleRValue *From, const ArrayType &arrayType,
      DataLocation ToLoc, DataLocation FromLoc,
      iele::IeleLocalVariable *SizeVariableTo, iele::IeleLocalVariable *SizeVariableFrom,
      iele::IeleLocalVariable *ElementTo, iele::IeleLocalVariable *ElementFrom,
      iele::IeleValue *AllocedValue);

  void appendCopy(
      IeleLValue *To, TypePointer ToType, IeleRValue *From,
      TypePointer FromType, DataLocation ToLoc, DataLocation FromLoc);

  // Infrastructure for copying recursive structs.
  std::map<std::string, std::map<std::string, iele::IeleFunction *>>
    RecursiveStructCopiers;
  iele::IeleFunction *getRecursiveStructCopier(
      const StructType &type, DataLocation FromLoc,
      const StructType &toType, DataLocation ToLoc);

  // Helper function that appends code for copying a struct.
  void appendStructCopy(
      const StructType &type, iele::IeleValue *Address, DataLocation FromLoc,
      const StructType &toType, iele::IeleValue *ToAddress, DataLocation ToLoc);

  void appendAccessorFunction(const VariableDeclaration *stateVariable);

  IeleLValue *appendGlobalVariable(iele::IeleValue *Identifier, std::string name,
                      bool isValueType, unsigned sizeInRegisters);

  IeleLValue *makeLValue(iele::IeleValue *Address, TypePointer type, DataLocation Loc, iele::IeleValue *Offset = nullptr);

  void appendLValueDelete(IeleLValue *LValue, TypePointer Type);

  // Infrastructure for deleting recursive structs.
  std::map<std::string, iele::IeleFunction *> RecursiveStructDestructors;
  iele::IeleFunction *getRecursiveStructDestructor(const StructType &type);

  // Helper function that appends code for deleting a struct.
  void appendStructDelete(const StructType &type, iele::IeleValue *Address);

  // Helper function that appends a loop for deleting array elements.
  void appendArrayDeleteLoop(
      const ArrayType &type, iele::IeleLocalVariable *ElementAddressVariable,
      iele::IeleLocalVariable *NumElementsVariable);

  void appendArrayLengthResize(iele::IeleValue *LValue, iele::IeleValue *NewLength);

  iele::IeleLocalVariable *appendBinaryOperator(
      Token Opcode, iele::IeleValue *LeftOperand,
      iele::IeleValue *RightOperand,
      TypePointer ResultType);

  void appendShiftBy(iele::IeleLocalVariable *Result, iele::IeleValue *Value,
      int shiftAmount);
  void appendMask(iele::IeleLocalVariable *Result, iele::IeleValue *Value, 
      int bytes, bool issigned);
  IeleRValue *appendTypeConversion(
      IeleRValue *Operand,
      TypePointer SourceType,
      TypePointer TargetType);

  void appendTypeConversions(
    llvm::SmallVectorImpl<IeleRValue *> &Results, 
    llvm::SmallVectorImpl<IeleRValue *> &RHSValues,
    TypePointer SourceType, TypePointers TargetTypes);

  iele::IeleValue *appendBooleanOperator(
      Token Opcode,
      const Expression &LeftOperand,
      const Expression &RightOperand);

  void appendConditional(
      iele::IeleValue *ConditionValue,
      llvm::SmallVectorImpl<IeleLValue *> &Results,
      const std::function<void(llvm::SmallVectorImpl<IeleRValue *> &)> &TrueExpression,
      const std::function<void(llvm::SmallVectorImpl<IeleRValue *> &)> &FalseExpression);

  void appendConditionalBranch(
    llvm::SmallVectorImpl<IeleRValue *> &Results,
    const Expression &Branch,
    TypePointer Type);

  iele::IeleLocalVariable *appendMemorySpill();

  bool shouldCopyStorageToStorage(const Type &ToType, const IeleLValue *To, const Type &From) const;
  bool shouldCopyMemoryToStorage(const Type &ToType, const IeleLValue *To, const Type &FromType) const;
  bool shouldCopyMemoryToMemory(const Type &ToType, const IeleLValue *To, const Type &FromType) const;
  bool shouldCopyStorageToMemory(const Type &ToType, const Type &FromType) const;

  bool isMostDerived(const FunctionDefinition *d) const;
  bool isMostDerived(const VariableDeclaration *d) const;
  const ContractDefinition *contractFor(const Declaration *d) const;
  const FunctionDefinition *superFunction(const FunctionDefinition &function, const ContractDefinition &contract);
  const ContractDefinition *superContract(const ContractDefinition &contract);
  const FunctionDefinition *resolveVirtualFunction(const FunctionDefinition &function);
  const FunctionDefinition *resolveVirtualFunction(const FunctionDefinition &function, std::vector<const ContractDefinition *>::iterator it);

  // IELE Runtime functionality. These methods append calls to the IELE memory
  // management runtime and indicate that the runtime should be included as part
  // of the compiling contract.
  iele::IeleLocalVariable *appendIeleRuntimeAllocateMemory(
      iele::IeleValue *NumSlots);
  iele::IeleLocalVariable *appendIeleRuntimeAllocateStorage(
      iele::IeleValue *NumSlots);
  void appendIeleRuntimeFill(
      iele::IeleValue *To, iele::IeleValue *NumSlots, iele::IeleValue *Value,
      DataLocation Loc);
  void appendIeleRuntimeCopy(
      iele::IeleValue *From, iele::IeleValue *To, iele::IeleValue *NumSlots,
      DataLocation FromLoc, DataLocation ToLoc);
  
  // Encoding functionality
  IeleRValue *encoding(
    IeleRValue *argument, 
    TypePointer type,
    bool hash = false);
  iele::IeleValue *encoding(
    llvm::SmallVectorImpl<IeleRValue *> &arguments,
    TypePointers types,
    iele::IeleLocalVariable *NextFree,
    bool appendWidths);

  IeleRValue *decoding(
    IeleRValue *encoded,
    TypePointer type);
  void decoding(
    IeleRValue *encoded,
    TypePointers types,
    llvm::SmallVectorImpl<IeleRValue *> &results);

  void doEncode(
    iele::IeleValue *NextFree,
    iele::IeleLocalVariable *CrntPos, IeleLValue *ArgValue,
    iele::IeleLocalVariable *ArgTypeSize, iele::IeleLocalVariable *ArgLen,
    TypePointer type, bool appendWidths, bool bigEndian,
    DataLocation Loc = DataLocation::CallData);
  void doDecode(
    iele::IeleValue *NextFree,
    iele::IeleLocalVariable *CrntPos, IeleLValue *StoreAt,
    iele::IeleLocalVariable *ArgTypeSize, iele::IeleLocalVariable *ArgLen,
    TypePointer type, DataLocation Loc = DataLocation::CallData);

  // Infrastructure for encoding/decoding recursive structs.
  std::map<std::string, std::map<bool, std::map<bool, iele::IeleFunction *>>>
    RecursiveStructEncoders;
  iele::IeleFunction *getRecursiveStructEncoder(
      const StructType &type, DataLocation Loc, bool appendWidths, bool bigEndian);
  std::map<std::string, iele::IeleFunction *> RecursiveStructDecoders;
  iele::IeleFunction *getRecursiveStructDecoder(const StructType &type, DataLocation Loc);

  // Helper functions that append code for encoding/decoding a struct.
  void appendStructEncode(
      const StructType &type, DataLocation Loc, iele::IeleValue *Address,
      iele::IeleLocalVariable *AddrTypeSize, iele::IeleLocalVariable *AddrLen,
      iele::IeleValue *NextFree, iele::IeleLocalVariable *CrntPos,
      bool appendWidths, bool bigEndian);
  void appendStructDecode(
      const StructType &type, DataLocation Loc, iele::IeleValue *Address,
      iele::IeleLocalVariable *AddrTypeSize, iele::IeleLocalVariable *AddrLen,
      iele::IeleValue *NextFree, iele::IeleLocalVariable *CrntPos);

  void appendByteWidth(iele::IeleLocalVariable *Result, iele::IeleValue *Value);
};

} // end namespace frontend
} // end namespace solidity
