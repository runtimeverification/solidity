#pragma once

#include "libiele/IeleFunction.h"

#include<unordered_map>
#include<unordered_set>

namespace solidity {
namespace iele {

namespace transform {

class IeleCFGBlock {
public:
  // The type for the list of instructions.
  using IeleCFGInstructionListType = std::vector<const IeleInstruction *>;

  // The instruction iterator.
  using iterator = IeleCFGInstructionListType::const_iterator;
  // The instruction reverse iterator.
  using reverse_iterator = IeleCFGInstructionListType::const_reverse_iterator;

private:
  IeleCFGInstructionListType IeleCFGInstructionList;
  const IeleBlock *TargetBlock;
  unsigned TargetStartIndex;
  unsigned TargetEndIndex;

public:
  IeleCFGBlock(const IeleBlock *B, unsigned StartIndex, unsigned EndIndex);

  std::string getName() const {
    return TargetBlock->getName().str() + std::string("_") + std::to_string(TargetStartIndex);
  }

  // Get the underlying elements of the IeleCFGBlock.
  inline const IeleBlock *getTargetBlock() const { return TargetBlock; }
  inline unsigned getTargetStartIndex() const { return TargetStartIndex; }
  inline unsigned getTargetEndIndex() const { return TargetEndIndex; }
  const IeleCFGInstructionListType &getIeleCFGInstructionList() const {
    return IeleCFGInstructionList;
  }

  // IeleCFGInstructionListType iterator forwarding functions.
  iterator begin() const { return IeleCFGInstructionList.begin(); }
  iterator end  () const { return IeleCFGInstructionList.end();   }

  reverse_iterator rbegin() const { return IeleCFGInstructionList.rbegin(); }
  reverse_iterator rend  () const { return IeleCFGInstructionList.rend();   }

  llvm::iterator_range<iterator> instructions() const {
    return llvm::make_range(begin(), end());
  }

  size_t  size() const { return IeleCFGInstructionList.size();  }
  bool   empty() const { return IeleCFGInstructionList.empty(); }

  const IeleInstruction *front() const { return IeleCFGInstructionList.front(); }
  const IeleInstruction  *back() const { return IeleCFGInstructionList.back();  }
};

class IeleCFG {
public:
  // The type for a list of IeleCFGBlocks.
  using IeleCFGBlockListType = std::vector<IeleCFGBlock>;

  // The type for a set of IeleCFGBlocks.
  using IeleCFGBlockSetType = std::unordered_set<const IeleCFGBlock *>;

  // Iterator over basic blocks in the graph.
  using iterator = IeleCFGBlockListType::const_iterator;

  // Iterator over basic block successors.
  using succ_iterator = IeleCFGBlockSetType::iterator;

  // Iterator over basic block predecessors.
  using pred_iterator = IeleCFGBlockSetType::iterator;

private:

  using iele_cfg_t =
    std::unordered_map<const IeleCFGBlock *, IeleCFGBlockSetType>;

  const IeleFunction *Function;
  const IeleCFGBlock *Entry;
  IeleCFGBlockListType Blocks;
  iele_cfg_t Successors;
  iele_cfg_t Predecessors;

  IeleCFG(const IeleFunction *F);

public:
  IeleCFG(const IeleCFG&) = delete;
  void operator=(const IeleCFG&) = delete;

  static IeleCFG *Create(const IeleFunction *F) {
    return new IeleCFG(F);
  }

  // IeleCFG iterator forwarding functions.
  iterator       begin()       { return Blocks.begin(); }
  iterator       end  ()       { return Blocks.end();   }

  llvm::iterator_range<iterator> blocks() {
    return llvm::make_range(begin(), end());
  }

  size_t  size() const { return Blocks.size();  }
  bool   empty() const { return Blocks.empty(); }

  const IeleCFGBlock *entry() const { return Entry; }

  // IeleCFG successor/predecessor iterator forwarding functions.
  succ_iterator succ_begin(const IeleCFGBlock *B) { return Successors[B].begin(); }
  succ_iterator succ_end  (const IeleCFGBlock *B) { return Successors[B].end();   }
  pred_iterator pred_begin(const IeleCFGBlock *B) { return Predecessors[B].begin(); }
  pred_iterator pred_end  (const IeleCFGBlock *B) { return Predecessors[B].end();   }

  llvm::iterator_range<succ_iterator> successors(const IeleCFGBlock *B) {
    return llvm::make_range(succ_begin(B), succ_end(B));
  }
  llvm::iterator_range<pred_iterator> predecessors(const IeleCFGBlock *B) {
    return llvm::make_range(pred_begin(B), pred_end(B));
  }

  size_t  succ_size(const IeleCFGBlock *B) { return Successors[B].size();  }
  bool   succ_empty(const IeleCFGBlock *B) { return Successors[B].empty(); }
  size_t  pred_size(const IeleCFGBlock *B) { return Predecessors[B].size();  }
  bool   pred_empty(const IeleCFGBlock *B) { return Predecessors[B].empty(); }

  void print(llvm::raw_ostream &OS, unsigned indent = 0);
};

} // end namespace transform
} // end namespace iele
} // end namespace solidity
