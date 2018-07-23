#include "IeleContract.h"

#include "IeleContext.h"
#include "IeleIntConstant.h"
#include "IeleValueSymbolTable.h"

#include <libsolidity/interface/Exceptions.h>
#include "llvm/ADT/Optional.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Program.h"
#include <llvm/Config/llvm-config.h>

#include <cstdio>
#include <cstdlib>
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
  IeleValue(Ctx, IeleValue::IeleContractVal),
  IncludeMemoryRuntime(false),
  IncludeStorageRuntime(false),
  NextFreePtrAddress(bigint(0)) {
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
  if (!IncludeMemoryRuntime && !IncludeStorageRuntime)
    return;

  if (IncludeStorageRuntime) {
    RuntimeSourceCode.append(
      "\n@ielert.storage.next.free = " + NextFreePtrAddress.str() + "\n");
    RuntimeSourceCode.append(
#include "iele-rt/iele-storage-rt.h"
    );
  }

  if (IncludeMemoryRuntime) {
    if (IncludeStorageRuntime)
      RuntimeSourceCode.append("\n");
    RuntimeSourceCode.append(
#include "iele-rt/iele-memory-rt.h"
    );
  }

  std::istringstream rtstream(RuntimeSourceCode);
  std::string line;
  std::string Indent(indent, ' ');
  OS << "\n";
  while (!rtstream.eof()) {
    std::getline(rtstream, line);
    OS << Indent << line << "\n";
  }
}

std::string IeleContract::escapeIeleName(const std::string &str) {
  std::string result;
  for (char c : str) {
    if (c == '"' || c == '\\' || !isprint(c)) {
      result.push_back('\\');
      char code[3];
      snprintf(code, 3, "%02x", (unsigned char)c);
      result.push_back(code[0]);
      result.push_back(code[1]);
    } else {
      result.push_back(c);
    }
  }
  return result;
}

void IeleContract::print(llvm::raw_ostream &OS, unsigned indent) const {
  std::string Indent(indent, ' ');
  for (const IeleContract *Contract : IeleContractList) {
    Contract->print(OS, indent);
    OS << "\n";
  }
  OS << Indent << "contract \"" << escapeIeleName(getName()) << "\" {\n";
  bool isFirst = true;
  for (const IeleContract *Contract : IeleContractList) {
    OS << "\n";
    if (!isFirst)
      OS << "\n";
    OS << "external contract \"";
    OS << escapeIeleName(Contract->getName());
    OS << "\"";
    isFirst = false;
  }
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

  int fd;
  char tempin[] = "/tmp/stdin-XXXXXX";
  fd = mkstemp(tempin);
  solAssert(fd != -1, "Cannot create temp file");

  char tempout[] = "/tmp/stdout-XXXXXX";
  fd = mkstemp(tempout);
  solAssert(fd != -1, "Cannot create temp file");

  std::ofstream write(tempin);
  write << assembly << endl;
  write.close();
  ErrorOr<std::string> result = findProgramByName("iele-assemble");
  if (error_code error = result.getError()) {
    solAssert(false, error.message());
  }
  std::string program = result.get();
  const char *args[] = {"iele-assemble", tempin, nullptr};
  const StringRef output = tempout;

#if defined(LLVM_VERSION_MAJOR) && ((LLVM_VERSION_MAJOR == 4) || (LLVM_VERSION_MAJOR == 5))
  const StringRef *redirects[] = {nullptr, &output, nullptr};
#endif

#if defined(LLVM_VERSION_MAJOR) && LLVM_VERSION_MAJOR == 6
  const Optional<StringRef> redirects[] = {None, Optional<StringRef>::create(&output), None};  
#endif

  int exit = ExecuteAndWait(program, args, nullptr, redirects);
  solAssert(exit == 0, "Iele assembler failed to execute on " + string(tempin));

  std::ifstream read(tempout);
  std::stringstream buffer;
  buffer << read.rdbuf();
  std::string hex = buffer.str();
  read.close();

  std::remove(tempin);
  std::remove(tempout);

  return fromHex(hex, WhenError::Throw) + AuxiliaryData;
}
