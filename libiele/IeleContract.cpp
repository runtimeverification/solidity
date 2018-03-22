#include "IeleContract.h"

#include "IeleContext.h"
#include "IeleIntConstant.h"
#include "IeleValueSymbolTable.h"

#include <libsolidity/interface/Exceptions.h>

#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Program.h"

#include <cstdio>
#include <fstream>
#include <sstream>

#include <boost/filesystem.hpp>

using namespace std;
using namespace dev;
using namespace dev::iele;
using namespace boost::filesystem;
using namespace llvm;
using namespace llvm::sys;

IeleContract::IeleContract(IeleContext *Ctx, const llvm::Twine &Name,
                           IeleContract *C) :
  IeleValue(Ctx, IeleValue::IeleContractVal), Parent(nullptr),
  IncludeStorageRuntime(false),
  IncludeMemoryRuntime(false) {
  SymTab = llvm::make_unique<IeleValueSymbolTable>();

  if (C) {
    C->getIeleContractList().push_back(this);
  }

  setName(Name);

  Ctx->OwnedContracts.insert(this);
}

IeleContract::~IeleContract() { }

void IeleContract::printRuntime(llvm::raw_ostream &OS, unsigned indent) const {
  std::string RuntimeSourceCode;
  if (!IncludeStorageRuntime && !IncludeMemoryRuntime)
    return;

  if (IncludeStorageRuntime)
    RuntimeSourceCode.append(
#include "iele-rt/iele-storage-rt.h"
    );
  if (IncludeMemoryRuntime)
    if (IncludeStorageRuntime)
      RuntimeSourceCode.push_back('\n');
    RuntimeSourceCode.append(
#include "iele-rt/iele-memory-rt.h"
    );

  std::istringstream rtstream(RuntimeSourceCode);
  std::string line;
  std::string Indent(indent, ' ');
  OS << "\n";
  while (!rtstream.eof()) {
    std::getline(rtstream, line);
    OS << Indent << line << "\n";
  }
}

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
  printRuntime(OS, indent);
  OS << "\n\n" << Indent << "}" << "\n";
}

bytes IeleContract::toBinary() const {
  std::string assembly;
  llvm::raw_string_ostream OS(assembly);
  this->print(OS);
  OS.flush();

  path tempin = unique_path();
  const std::string tempin_str = tempin.native();
  path tempout = unique_path();
  const std::string tempout_str = tempout.native();

  std::ofstream write(tempin_str);
  write << assembly << endl;
  write.close();
  std::string program = findProgramByName("iele-assemble").get();
  const char *args[] = {"iele-assemble", "-", nullptr};
  const StringRef input = tempin_str;
  const StringRef output = tempout_str;
  const StringRef *redirects[] = {&input, &output, nullptr};
  int exit = ExecuteAndWait(program, args, nullptr, redirects);
  solAssert(exit == 0, "Iele assembler failed to execute on " + tempin_str);

  std::ifstream read(tempout_str);
  std::stringstream buffer;
  buffer << read.rdbuf();
  std::string hex = buffer.str();
  read.close();

  std::remove(tempin_str.c_str());
  std::remove(tempout_str.c_str());
  
  return fromHex(hex, WhenError::Throw);
}
