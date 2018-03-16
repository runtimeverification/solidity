#include "IeleBlock.h"

#include "IeleFunction.h"

#include <libsolidity/interface/Exceptions.h>

using namespace dev;
using namespace dev::iele;

IeleBlock::IeleBlock(IeleContext *Ctx, const llvm::Twine &Name,
                     IeleFunction *F, IeleBlock *InsertBefore) :
  IeleValue(Ctx, IeleValue::IeleBlockVal), Parent(nullptr) {
  if (F)
    insertInto(F, InsertBefore);
  else
    solAssert(!InsertBefore,
           "Cannot insert block before another block with no function!");

  setName(Name);
}

void IeleBlock::insertInto(IeleFunction *NewParent, IeleBlock *InsertBefore) {
  solAssert(NewParent, "Expected a parent");
  solAssert(!Parent, "Already has a parent");

  if (InsertBefore)
    NewParent->getIeleBlockList().insert(
        InsertBefore->
          llvm::ilist_node_with_parent<IeleBlock, IeleFunction>::getIterator(),
        this);
  else
    NewParent->getIeleBlockList().push_back(this);
}

void IeleBlock::setParent(IeleFunction *parent) {
  // Assert that all operands and lvalues of the block's instructions belong to
  // the function.
  for (const IeleInstruction &I : instructions()) {
    for (const IeleValue *V : I.operands()) {
      if (const IeleLocalVariable *LV = llvm::dyn_cast<IeleLocalVariable>(V))
        solAssert(LV->getParent() == parent,
               "Instruction operand belongs to a different function!");
      else if (const IeleBlock *B = llvm::dyn_cast<IeleBlock>(V))
        solAssert(B->getParent() == parent,
               "Instruction operand belongs to a different function!");
    }

    for (const IeleLocalVariable *LV : I.lvalues()) {
      solAssert(LV->getParent() == parent,
             "Instruction lvalue belongs to a different function!");
    }
  }

  // Set the parent.
  Parent = parent;
}

IeleValueSymbolTable *IeleBlock::getIeleValueSymbolTable() {
  if (IeleFunction *F = getParent())
    return F->getIeleValueSymbolTable();
  return nullptr;
}

const IeleValueSymbolTable *IeleBlock::getIeleValueSymbolTable() const {
  if (const IeleFunction *F = getParent())
    return F->getIeleValueSymbolTable();
  return nullptr;
}

bool IeleBlock::endsWithRet() const {
  return !empty() && back().getOpcode() == IeleInstruction::Ret;
}

IeleBlock::~IeleBlock() { }

void IeleBlock::print(llvm::raw_ostream &OS, unsigned indent) const {
  std::string Indent(indent, ' ');
  OS << Indent << getName() << ":";
  for (const IeleInstruction &I : instructions()) {
    OS << "\n";
    I.print(OS, indent + 2);
  }
}
