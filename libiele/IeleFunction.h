#pragma once

#include "IeleArgument.h"
#include "IeleBlock.h"
#include "IeleGlobalValue.h"

namespace solidity {
namespace iele {

class IeleFunction :
  public IeleGlobalValue,
  public llvm::ilist_node_with_parent<IeleFunction, IeleContract> {
public:
  // The type for the list of blocks.
  using IeleBlockListType = SymbolTableList<IeleBlock>;
  // The type for the list of formal arguments.
  using IeleArgumentListType = SymbolTableList<IeleArgument>;
  // The type for the list of local variables.
  using IeleLocalVariableListType = SymbolTableList<IeleLocalVariable>;

  // The block iterator.
  using iterator = IeleBlockListType::iterator;
  // The block constant iterator.
  using const_iterator = IeleBlockListType::const_iterator;

  // The formal argument iterator.
  using arg_iterator = IeleArgumentListType::iterator;
  // The formal argument constant iterator.
  using const_arg_iterator = IeleArgumentListType::const_iterator;

  // The local variable iterator.
  using lvar_iterator = IeleLocalVariableListType::iterator;
  // The local variable constant iterator.
  using const_lvar_iterator = IeleLocalVariableListType::const_iterator;

private:
  IeleBlockListType IeleBlockList;
  IeleArgumentListType IeleArgumentList;
  IeleLocalVariableListType IeleLocalVariableList;
  std::unique_ptr<IeleValueSymbolTable> SymTab;
  bool IsPublic;
  bool IsInit;
  bool IsDeposit;

  friend class SymbolTableListTraits<IeleFunction>;

  // IeleFunction ctor - If the (optional) IeleContract argument is specified,
  // the function is automatically inserted into the end of the function list
  // for the given contract.
  //
  IeleFunction(IeleContext *Ctx, bool isPublic, bool isInit, bool isDeposit,
               const llvm::Twine &Name= "", IeleContract *C = nullptr);

public:
  IeleFunction(const IeleFunction&) = delete;
  void operator=(const IeleFunction&) = delete;

  ~IeleFunction() override;

  static IeleFunction *Create(IeleContext *Ctx, bool isPublic,
                              const llvm::Twine &Name = "",
                              IeleContract *C = nullptr) {
    return new IeleFunction(Ctx, isPublic, false, false, Name, C);
  }

  static IeleFunction *CreateInit(IeleContext *Ctx, IeleContract *C = nullptr) {
    return new IeleFunction(Ctx, false, true, false, "init", C);
  }

  static IeleFunction *CreateDeposit(IeleContext *Ctx, bool isPublic,
                                     IeleContract *C = nullptr) {
    return new IeleFunction(Ctx, isPublic, false, true, "deposit", C);
  }

  inline bool isPublic() const { return IsPublic; }
  inline void setPublic(bool isPublic) { IsPublic = isPublic; }

  inline bool isInit() const { return IsInit; }
  inline void setInit(bool isInit) { IsInit = isInit; }

  inline bool isDeposit() const { return IsDeposit; }
  inline void setDeposit(bool isDeposit) { IsDeposit = isDeposit; }

  // Get the underlying elements of the IeleFunction.
  //
  const IeleBlockListType &getIeleBlockList() const {
    return IeleBlockList;
  }
  IeleBlockListType &getIeleBlockList() {
    return IeleBlockList;
  }

  static IeleBlockListType
  IeleFunction::*getSublistAccess(IeleBlock*) {
    return &IeleFunction::IeleBlockList;
  }

  const IeleArgumentListType &getIeleArgumentList() const {
    return IeleArgumentList;
  }
  IeleArgumentListType &getIeleArgumentList() {
    return IeleArgumentList;
  }

  static IeleArgumentListType
  IeleFunction::*getSublistAccess(IeleArgument*) {
    return &IeleFunction::IeleArgumentList;
  }

  const IeleLocalVariableListType &getIeleLocalVariableList() const {
    return IeleLocalVariableList;
  }
  IeleLocalVariableListType &getIeleLocalVariableList() {
    return IeleLocalVariableList;
  }

  static IeleLocalVariableListType
  IeleFunction::*getSublistAccess(IeleLocalVariable*) {
    return &IeleFunction::IeleLocalVariableList;
  }

  // Return the symbol table if any, otherwise nullptr.
  //
  inline IeleValueSymbolTable *getIeleValueSymbolTable() {
    return SymTab.get();
  }
  inline const IeleValueSymbolTable *getIeleValueSymbolTable() const {
    return SymTab.get();
  }

  // IeleBlock iterator forwarding functions.
  //
  iterator       begin()       { return IeleBlockList.begin(); }
  const_iterator begin() const { return IeleBlockList.begin(); }
  iterator       end  ()       { return IeleBlockList.end();   }
  const_iterator end  () const { return IeleBlockList.end();   }

  llvm::iterator_range<iterator> blocks() {
    return make_range(begin(), end());
  }
  llvm::iterator_range<const_iterator> blocks() const {
    return make_range(begin(), end());
  }

  size_t  size() const { return IeleBlockList.size();  }
  bool   empty() const { return IeleBlockList.empty(); }

  const IeleBlock &front() const { return IeleBlockList.front(); }
        IeleBlock &front()       { return IeleBlockList.front(); }
  const IeleBlock  &back() const { return IeleBlockList.back();  }
        IeleBlock  &back()       { return IeleBlockList.back();  }

  // IeleArgument iterator forwarding functions.
  //
  arg_iterator arg_begin() {
    return IeleArgumentList.begin();
  }
  const_arg_iterator arg_begin() const {
    return IeleArgumentList.begin();
  }
  arg_iterator arg_end() {
    return IeleArgumentList.end();
  }
  const_arg_iterator arg_end() const {
    return IeleArgumentList.end();
  }

  llvm::iterator_range<arg_iterator> args() {
    return make_range(arg_begin(), arg_end());
  }
  llvm::iterator_range<const_arg_iterator> args() const {
    return make_range(arg_begin(), arg_end());
  }

  size_t  arg_size() const { return IeleArgumentList.size();  }
  bool   arg_empty() const { return IeleArgumentList.empty(); }

  // IeleLocalVariable iterator forwarding functions.
  //
  lvar_iterator lvar_begin() {
    return IeleLocalVariableList.begin();
  }
  const_lvar_iterator lvar_begin() const {
    return IeleLocalVariableList.begin();
  }
  lvar_iterator lvar_end() {
    return IeleLocalVariableList.end();
  }
  const_lvar_iterator lvar_end() const {
    return IeleLocalVariableList.end();
  }

  llvm::iterator_range<lvar_iterator> lvars() {
    return make_range(lvar_begin(), lvar_end());
  }
  llvm::iterator_range<const_lvar_iterator> lvars() const {
    return make_range(lvar_begin(), lvar_end());
  }

  size_t  lvar_size() const { return IeleLocalVariableList.size();  }
  bool   lvar_empty() const { return IeleLocalVariableList.empty(); }

  // Helper that prints the function's name as it should appear in the IELE
  // textual format.
  void printNameAsIeleText(llvm::raw_ostream &OS) const;

  void print(llvm::raw_ostream &OS, unsigned indent = 0) const override;

  // Method for support type inquiry through isa, cast, and dyn_cast.
  static bool classof(const IeleValue *V) {
    return V->getIeleValueID() == IeleValue::IeleFunctionVal;
  }
};

} // end namespace iele
} // end namespace solidity
