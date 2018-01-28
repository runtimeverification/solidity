#pragma once

#include "IeleValue.h"

#include "llvm/ADT/StringMap.h"

namespace llvm {

template <unsigned InternalLen> class SmallString;
class StringRef;

} // end namespace llvm

namespace dev {
namespace iele {

class IeleInstruction;
template <typename ItemClass> class SymbolTableListTraits;

// This class provides a symbol table of name/value pairs. It is essentially
// a std::map<std::string,Value*> but has a controlled interface provided by
// LLVM as well as ensuring uniqueness of names.
//
class IeleValueSymbolTable {
  friend class SymbolTableListTraits<IeleArgument>;
  friend class SymbolTableListTraits<IeleBlock>;
  friend class SymbolTableListTraits<IeleContract>;
  friend class SymbolTableListTraits<IeleFunction>;
  friend class SymbolTableListTraits<IeleGlobalVariable>;
  friend class SymbolTableListTraits<IeleInstruction>;
  friend class SymbolTableListTraits<IeleLocalVariable>;
  friend class IeleValue;

public:
  // A mapping of names to iele values.
  using IeleValueMap = llvm::StringMap<IeleValue*>;

  // An iterator over a IeleValueMap.
  using iterator = IeleValueMap::iterator;
  using const_iterator = IeleValueMap::const_iterator;

  // Constructors
  IeleValueSymbolTable() : vmap(0) { }
  ~IeleValueSymbolTable();

  // Accessors

  // This method finds the value with the given Name in the the symbol table
  // and returns the iele value associated with the Name.
  IeleValue *lookup(llvm::StringRef Name) const { return vmap.lookup(Name); }

  // Returns true iff the symbol table is empty.
  inline bool empty() const { return vmap.empty(); }

  // The number of name/type pairs is returned.
  inline unsigned size() const { return unsigned(vmap.size()); }

  // Iteration
  inline iterator begin() { return vmap.begin(); }
  inline const_iterator begin() const { return vmap.begin(); }
  inline iterator end() { return vmap.end(); }
  inline const_iterator end() const { return vmap.end(); }

  // Mutators
private:
  IeleName *makeUniqueName(IeleValue *V, llvm::SmallString<256> &UniqueName);

  // This method adds the provided iele value to the symbol table. The value
  // must have a name which is used to place the value in the symbol table.
  // If the inserted name conflicts, this renames the value.
  void reinsertIeleValue(IeleValue *V);

  // This method attempts to create a IeleName and insert it into the symbol
  // table with the specified name. If it conflicts, it auto-renames the name
  // and returns that instead.
  IeleName *createIeleName(llvm::StringRef Name, IeleValue *V);

  // This method removes a iele value from the symbol table. It leaves the
  // IeleName attached to the value, but it is no longer inserted in the
  // symtab.
  void removeIeleName(IeleName *V);

  // Internal Data
  IeleValueMap vmap;                // The map that holds the symbol table.
  mutable uint32_t LastUnique = 0;  // Counter for tracking unique names
};

} // end namespace iele
} // end namespace dev
