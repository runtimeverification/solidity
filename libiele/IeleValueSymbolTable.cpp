#include "IeleValueSymbolTable.h"

#include <libsolidity/interface/Exceptions.h>

#include "llvm/ADT/SmallString.h"

using namespace dev;
using namespace dev::iele;

IeleValueSymbolTable::~IeleValueSymbolTable() { }

IeleName *IeleValueSymbolTable::makeUniqueName(
    IeleValue *V, llvm::SmallString<256> &UniqueName) {
  unsigned BaseSize = UniqueName.size();
  while (true) {
    // Trim any suffix off and append the next number.
    UniqueName.resize(BaseSize);
    if (llvm::isa<IeleGlobalValue>(V))
      UniqueName.append(".");
    UniqueName.append(std::to_string(++LastUnique));

    // Try insert the vmap entry with this suffix.
    auto IterBool = vmap.insert(std::make_pair(UniqueName, V));
    if (IterBool.second)
      return &*IterBool.first;
  }
}

void IeleValueSymbolTable::reinsertIeleValue(IeleValue* V) {
  solAssert(V->hasName(), "Can't insert nameless Value into symbol table");

  // Try inserting the name, assuming it won't conflict.
  if (vmap.insert(V->getIeleName())) {
    return;
  }
  
  // Otherwise, there is a naming conflict.  Rename this value.
  llvm::SmallString<256> UniqueName(V->getName().begin(), V->getName().end());

  // The name is also already used, just free it so we can allocate a new name.
  V->getIeleName()->Destroy();

  IeleName *VN = makeUniqueName(V, UniqueName);
  V->setIeleName(VN);
}

void IeleValueSymbolTable::removeIeleName(IeleName *V) {
  vmap.remove(V);
}

IeleName *IeleValueSymbolTable::createIeleName(llvm::StringRef Name,
                                               IeleValue *V) {
  // In the common case, the name is not already in the symbol table.
  auto IterBool = vmap.insert(std::make_pair(Name, V));
  if (IterBool.second) {
    return &*IterBool.first;
  }
  
  // Otherwise, there is a naming conflict. Rename this value.
  llvm::SmallString<256> UniqueName(Name.begin(), Name.end());
  return makeUniqueName(V, UniqueName);
}
