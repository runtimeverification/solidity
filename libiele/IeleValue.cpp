#include "IeleValue.h"

#include "IeleArgument.h"
#include "IeleBlock.h"
#include "IeleContext.h"
#include "IeleContract.h"
#include "IeleFunction.h"
#include "IeleLocalVariable.h"
#include "IeleValueSymbolTable.h"

#include "llvm/ADT/SmallString.h"

using namespace dev;
using namespace dev::iele;

static bool getSymTab(IeleValue *V, IeleValueSymbolTable *&ST) {
  ST = nullptr;
  if (IeleContract *C = llvm::dyn_cast<IeleContract>(V)) {
    if (IeleContract *P = C->getParent())
      ST = P->getIeleValueSymbolTable();
  } else if (IeleBlock *B = llvm::dyn_cast<IeleBlock>(V)) {
    if (IeleFunction *P = B->getParent())
      ST = P->getIeleValueSymbolTable();
  } else if (IeleGlobalValue *GV = llvm::dyn_cast<IeleGlobalValue>(V)) {
    if (IeleContract *P = GV->getParent())
      ST = P->getIeleValueSymbolTable();
  } else if (IeleLocalVariable *LV = llvm::dyn_cast<IeleLocalVariable>(V)) {
    if (IeleFunction *P = LV->getParent())
      ST = P->getIeleValueSymbolTable();
  } else {
    assert(llvm::isa<IeleConstant>(V) && "Unknown value type!");
    return true;  // no name is setable for this.
  }
  return false;
}

IeleValue::~IeleValue() { }

IeleName *IeleValue::getIeleName() const {
  if (!HasName) return nullptr;

  auto I = Context->IeleNames.find(this);
  assert(I != Context->IeleNames.end() &&
         "No name entry found!");

  return I->second;
}

void IeleValue::setIeleName(IeleName *IN) {
  assert(HasName == Context->IeleNames.count(this) &&
         "HasName bit out of sync!");

  if (!IN) {
    if (HasName)
      Context->IeleNames.erase(this);
    HasName = false;
    return;
  }

  HasName = true;
  Context->IeleNames[this] = IN;
}

llvm::StringRef IeleValue::getName() const {
  // Make sure the empty string is still a C string (null terminated).
  if (!hasName())
    return llvm::StringRef("", 0);
  return getIeleName()->getKey();
}

void IeleValue::setName(const llvm::Twine &Name) {
  // Fast path when there is no name.
  if (Name.isTriviallyEmpty() && !hasName())
    return;

  llvm::SmallString<256> NameData;
  llvm::StringRef NameRef = Name.toStringRef(NameData);
  assert(NameRef.find_first_of(0) == llvm::StringRef::npos &&
         "Null bytes are not allowed in names");

  // Name isn't changing?
  if (getName() == NameRef)
    return;

  // Get the symbol table to update for this object.
  IeleValueSymbolTable *ST;
  if (getSymTab(this, ST))
    return;  // Cannot set a name on this value (e.g. constant).

  if (!ST) { // No symbol table to update?  Just do the change.
    if (NameRef.empty()) {
      // Free the name for this value.
      destroyIeleName();
      return;
    }

    // NOTE: Could optimize for the case the name is shrinking to not deallocate
    // then reallocate.
    destroyIeleName();

    // Create the new name.
    setIeleName(IeleName::Create(NameRef));
    getIeleName()->setValue(this);
    return;
  }

  // NOTE: Could optimize for the case the name is shrinking to not deallocate
  // then reallocate.
  if (hasName()) {
    // Remove old name.
    ST->removeIeleName(getIeleName());
    destroyIeleName();

    if (NameRef.empty())
      return;
  }

  // Name is changing to something new.
  setIeleName(ST->createIeleName(NameRef, this));
}

void IeleValue::destroyIeleName() {
  IeleName *Name = getIeleName();
  if (Name)
    Name->Destroy();
  setIeleName(nullptr);
}
