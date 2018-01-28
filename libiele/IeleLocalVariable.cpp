#include "IeleLocalVariable.h"

#include "IeleFunction.h"

using namespace dev;
using namespace dev::iele;

IeleLocalVariable::IeleLocalVariable(
    IeleContext *Ctx, const llvm::Twine &Name, IeleFunction *F,
    bool isArgument) :
  IeleValue(Ctx, isArgument ? IeleValue::IeleArgumentVal :
                              IeleValue::IeleLocalVariableVal),
  Parent(nullptr) {
  if (F && !isArgument)
    F->getIeleLocalVariableList().push_back(this);
  setName(Name);
}

IeleLocalVariable::~IeleLocalVariable() { }

void IeleLocalVariable::print(llvm::raw_ostream &OS, unsigned indent) const {
  std::string Indent(indent, ' ');
  OS << Indent << "%" << getName();
}
