#include "IeleIntConstant.h"


// https://stackoverflow.com/questions/5919996/how-to-detect-reliably-mac-os-x-ios-linux-windows-in-c-preprocessor

#ifdef _WIN64
//define something for Windows (64-bit)
#elif _WIN32
//define something for Windows (32-bit)
#elif __APPLE__
  #include "/usr/local/Cellar/boost/1.67.0_1/include/boost/format.hpp"
#elif __linux
  #include "/usr/include/boost/format.hpp"
#elif __unix // all unices not caught above
// Unix
#elif __posix
// POSIX
#endif


// On Linux:
//#include "/usr/include/boost/format.hpp"
// On Darwin (OSX) use this instead
//#include "/usr/local/Cellar/boost/1.67.0_1/include/boost/format.hpp"

#include "IeleContext.h"
#include "llvm/Support/raw_ostream.h"

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
