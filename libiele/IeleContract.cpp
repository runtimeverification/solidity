#include "IeleContract.h"

#include "IeleContext.h"
#include "IeleIntConstant.h"
#include "IeleValueSymbolTable.h"

using namespace dev;
using namespace dev::iele;

IeleContract::IeleContract(IeleContext *Ctx, const llvm::Twine &Name,
                           IeleContract *C) :
  IeleValue(Ctx, IeleValue::IeleContractVal), Parent(nullptr) {
  SymTab = llvm::make_unique<IeleValueSymbolTable>();

  if (C) {
    C->getIeleContractList().push_back(this);
  }

  setName(Name);

  Ctx->OwnedContracts.insert(this);
}

IeleContract::~IeleContract() { }

void IeleContract::print(llvm::raw_ostream &OS, unsigned indent) const {
  std::string Indent(indent, ' ');
  OS << Indent << "contract " << getName() << " {\n";
  bool isFirst = true;
  for (const IeleGlobalVariable &GV : globals()) {
    OS << "\n";
    if (!isFirst)
      OS << "\n";
    GV.print(OS, indent);
    OS << " = ";
    GV.getStorageAddress()->print(OS);
    isFirst = false;
  }
  for (const IeleFunction &F : functions()) {
    OS << "\n";
    if (!isFirst)
      OS << "\n";
    F.print(OS, indent);
    isFirst = false;
  }
  OS << "\n\n" << Indent << "}";
}
