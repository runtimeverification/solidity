#include "IeleArgument.h"

#include "IeleFunction.h"

using namespace solidity;
using namespace solidity::iele;

IeleArgument::IeleArgument(IeleContext *Ctx, const llvm::Twine &Name,
                           IeleFunction *F) :
  IeleLocalVariable(Ctx, Name, F, true) {
  if (F)
    F->getIeleArgumentList().push_back(this);
}

IeleArgument::~IeleArgument() { }
