#include "IeleIntConstant.h"

#include "IeleContext.h"

using namespace dev;
using namespace dev::iele;

IeleIntConstant::IeleIntConstant(IeleContext *Ctx, const llvm::APInt &V) :
  IeleConstant(Ctx, IeleValue::IeleIntConstantVal), Val(V) { }


IeleIntConstant *IeleIntConstant::Create(IeleContext *Ctx,
                                         const llvm::APInt &V) {
  std::unique_ptr<IeleIntConstant> &Slot = Ctx->IeleIntConstants[V];
  if (!Slot) {
    Slot.reset(new IeleIntConstant(Ctx, V));
  }
  return Slot.get();
}

IeleIntConstant::~IeleIntConstant() { }

IeleIntConstant *IeleIntConstant::getZero(IeleContext *Ctx) {
  return Create(Ctx, llvm::APInt(64, 0, true));
}

IeleIntConstant *IeleIntConstant::getOne(IeleContext *Ctx) {
  return Create(Ctx, llvm::APInt(64, 1, true));
}

IeleIntConstant *IeleIntConstant::getMinusOne(IeleContext *Ctx) {
  return Create(Ctx, llvm::APInt(64, UINT64_MAX, true));
}

void IeleIntConstant::print(llvm::raw_ostream &OS, unsigned indent) const {
  std::string Indent(indent, ' ');
  OS << Indent << Val;
}
