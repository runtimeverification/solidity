#pragma once

#include "libiele/IeleIntConstant.h"

namespace dev
{
namespace solidity
{

class IeleRValue
{
private:
  explicit IeleRValue(std::vector<iele::IeleValue *> Values) : Values(Values) {}
  std::vector<iele::IeleValue *> Values;

public:
  static IeleRValue *Create(std::vector<iele::IeleValue *> Var) {
    return new IeleRValue(Var);
  }

  static IeleRValue *CreateZero(iele::IeleContext *Ctx, unsigned Num) {
    std::vector<iele::IeleValue *> Values;
    for (unsigned i = 0; i < Num; i++) {
      Values.push_back(iele::IeleIntConstant::getZero(Ctx));
    }
    return new IeleRValue(Values);
  }

  const std::vector<iele::IeleValue *> &getValues() { return Values; }
  iele::IeleValue *getValue() { solAssert(Values.size() == 1, "required only one iele value"); return Values[0]; }

};

}

}
