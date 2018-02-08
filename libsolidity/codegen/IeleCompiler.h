#pragma once

#include "libsolidity/ast/ASTVisitor.h"

namespace dev {
namespace iele {

class IeleContract;

} // end namespace iele

namespace solidity {

class IeleCompiler : public ASTConstVisitor {
public:
  explicit IeleCompiler() : compiledContract(nullptr) { }

  // Compiles a contract.
  void compileContract(
      ContractDefinition const &contract,
      std::map<ContractDefinition const*, iele::IeleContract const*> const &contracts);

  // Returns the compiled IeleContract.
  iele::IeleContract const* ieleContract() const { return compiledContract; }

private:
  const iele::IeleContract *compiledContract;
};

} // end namespace solidity
} // end namespace dev
