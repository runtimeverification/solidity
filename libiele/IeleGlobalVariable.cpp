#include "IeleGlobalVariable.h"

#include "IeleContract.h"
#include "IeleIntConstant.h"

using namespace dev;
using namespace dev::iele;

IeleGlobalVariable::IeleGlobalVariable(IeleContext *Ctx,
                                       const llvm::Twine &Name,
                                       IeleContract *C) :
  IeleGlobalValue(Ctx, Name, IeleValue::IeleGlobalVariableVal),
  StorageAddress(nullptr) {
  if (C) {
    C->getIeleGlobalVariableList().push_back(this);
  }
}

IeleGlobalVariable::~IeleGlobalVariable() { }

void IeleGlobalVariable::print(llvm::raw_ostream &OS, unsigned indent) const {
  std::string Indent(indent, ' ');
  OS << Indent << "@\"" << IeleContract::escapeIeleName(getName()) << "\"";
}
