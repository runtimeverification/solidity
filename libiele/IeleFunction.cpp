#include "IeleFunction.h"

#include "IeleContract.h"
#include "IeleValueSymbolTable.h"

using namespace dev;
using namespace dev::iele;

IeleFunction::IeleFunction(IeleContext *Ctx, bool isPublic, bool isInit,
                           bool isDeposit, const llvm::Twine &Name,
                           IeleContract *C) :
  IeleGlobalValue(Ctx, Name, IeleValue::IeleFunctionVal),
  IsPublic(isPublic),
  IsInit(isInit),
  IsDeposit(isDeposit) {
  SymTab = llvm::make_unique<IeleValueSymbolTable>();

  if (C) {
    C->getIeleFunctionList().push_back(this);
  }
}

IeleFunction::~IeleFunction() { }

void IeleFunction::printNameAsIeleText(llvm::raw_ostream &OS) const {
  if (!(isInit() || isDeposit()))
    OS << "@\"" << getName() << "\"";
  else
    OS << "@" << getName();
}

void IeleFunction::print(llvm::raw_ostream &OS, unsigned indent) const {
  std::string Indent(indent, ' ');
  OS << Indent << "define ";
  if (isPublic())
    OS << "public ";
  printNameAsIeleText(OS);
  OS << "(";
  bool isFirst = true;
  for (const IeleArgument &A : args()) {
    if (!isFirst)
      OS << ", ";
    A.print(OS);
    isFirst = false;
  }
  OS << ") {\n";
  isFirst = true;
  for (const IeleBlock &B : blocks()) {
    if (!isFirst)
      OS << "\n\n";
    B.print(OS, indent);
    isFirst = false;
  }
  OS << "\n" << Indent << "}";
}
