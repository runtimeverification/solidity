#include "IeleIntConstant.h"

#include "IeleContext.h"

using namespace dev;
using namespace dev::iele;

IeleIntConstant::IeleIntConstant(IeleContext *Ctx, const bigint &V, bool H) :
  IeleConstant(Ctx, IeleValue::IeleIntConstantVal), Val(V), PrintAsHex(H) { }


IeleIntConstant *IeleIntConstant::Create(IeleContext *Ctx,
                                         const bigint &V,
                                         bool PrintAsHex) {

  std::unique_ptr<IeleIntConstant> &Slot = Ctx->IeleIntConstants[V];
  if (!Slot) {
    Slot.reset(new IeleIntConstant(Ctx, V, PrintAsHex));
  }
  return Slot.get();
}

IeleIntConstant::~IeleIntConstant() { }

IeleIntConstant *IeleIntConstant::getZero(IeleContext *Ctx) {
  return Create(Ctx, bigint(0), false);
}

IeleIntConstant *IeleIntConstant::getOne(IeleContext *Ctx) {
  return Create(Ctx, bigint(1), false);
}

IeleIntConstant *IeleIntConstant::getMinusOne(IeleContext *Ctx) {
  return Create(Ctx, bigint(-1), false);
}

void IeleIntConstant::print(llvm::raw_ostream &OS, unsigned indent) const {
  std::string Indent(indent, ' ');
  if (PrintAsHex) {
    OS << boost::str(boost::format("%1$#x") % Val);
  } else {
    OS << Indent << Val.str();
  }
}