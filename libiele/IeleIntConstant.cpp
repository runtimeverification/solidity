#include "IeleIntConstant.h"

#include "IeleContext.h"

using namespace dev;
using namespace dev::iele;

IeleIntConstant::IeleIntConstant(IeleContext *Ctx, const bigint &V) :
  IeleConstant(Ctx, IeleValue::IeleIntConstantVal), Val(V) { }


IeleIntConstant *IeleIntConstant::Create(IeleContext *Ctx,
                                         const bigint &V) {
  std::unique_ptr<IeleIntConstant> &Slot = Ctx->IeleIntConstants[V];
  if (!Slot) {
    Slot.reset(new IeleIntConstant(Ctx, V));
  }
  return Slot.get();
}

IeleIntConstant::~IeleIntConstant() { }

IeleIntConstant *IeleIntConstant::getZero(IeleContext *Ctx) {
  return Create(Ctx, bigint(0));
}

IeleIntConstant *IeleIntConstant::getOne(IeleContext *Ctx) {
  return Create(Ctx, bigint(1));
}

IeleIntConstant *IeleIntConstant::getMinusOne(IeleContext *Ctx) {
  return Create(Ctx, bigint(-1));
}

void IeleIntConstant::print(llvm::raw_ostream &OS, unsigned indent) const {
  std::string Indent(indent, ' ');
  OS << Indent << Val.str();
}
