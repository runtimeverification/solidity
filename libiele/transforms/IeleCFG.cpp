#include "IeleCFG.h"

#include <liblangutil/Exceptions.h>

#include "llvm/Support/raw_ostream.h"

using namespace std;
using namespace solidity;
using namespace solidity::iele;
using namespace solidity::iele::transform;

IeleCFGBlock::IeleCFGBlock(const IeleBlock *B, unsigned StartIndex, unsigned EndIndex) {
  solAssert(B, "Invalid target IeleBlock!");
  solAssert((StartIndex == 0 && EndIndex == 0 && B->empty()) ||
            (StartIndex <= EndIndex && EndIndex < B->size()),
            "Invalid start/end indices for target block!");
  TargetBlock = B;
  TargetStartIndex = StartIndex;
  TargetEndIndex = EndIndex;
  if (!B->empty()) {
    const auto &TargetList = B->getIeleInstructionList();
    auto It = TargetList.begin();
    for (unsigned i = 0; i < StartIndex; ++i) ++It;
    for (unsigned i = StartIndex; i <= EndIndex; ++i) {
      const IeleInstruction *Instr = &*It;
      IeleCFGInstructionList.push_back(Instr);
      ++It;
    }
  }
}

static bool isTerminator(const IeleInstruction *Instr) {
  return Instr->getOpcode() == IeleInstruction::Br ||
         Instr->getOpcode() == IeleInstruction::Ret ||
         Instr->getOpcode() == IeleInstruction::Revert;
}

static bool isConditionalBranch(const IeleInstruction *Instr) {
  return Instr->getOpcode() == IeleInstruction::Br && Instr->size() == 2;
}

static bool isUnconditionalTerminator(const IeleInstruction *Instr) {
  return isTerminator(Instr) && !isConditionalBranch(Instr);
}

IeleCFG::IeleCFG(const IeleFunction *F): Function(F) {
  // Create the IeleCFGBlocks and keep a map to them.
  std::unordered_map<const IeleBlock *, std::unordered_map<unsigned, unsigned>> BlockMap;
  for (const IeleBlock &TargetBlock : F->blocks()) {
    // Empty IeleBlock.
    if (TargetBlock.size() == 0) {
      Blocks.push_back(IeleCFGBlock(&TargetBlock, 0, 0));
      BlockMap[&TargetBlock][0] = Blocks.size() - 1;
      continue;
    }

    // Non-empty IeleBlock.
    unsigned Index = 0, TargetIndex = 0;
    auto It = TargetBlock.begin();
    const IeleInstruction *Instr = nullptr;

    while (Index < TargetBlock.size()) {
      Instr = &*It;

      // Include all non-terminator instructions.
      while (Index < TargetBlock.size() && !isTerminator(Instr)) {
        ++It; ++Index;
        if (Index < TargetBlock.size()) Instr = &*It;
      }
      // Include all conditional branch instructions.
      while (Index < TargetBlock.size() && isConditionalBranch(Instr)) {
        ++It; ++Index;
        if (Index < TargetBlock.size()) Instr = &*It;
      }
      // Create a new IeleCFGBlock.
      if (Index == TargetBlock.size()) {
        // End of the IeleBlock reached.
        Blocks.push_back(IeleCFGBlock(&TargetBlock, TargetIndex, Index - 1));
        BlockMap[&TargetBlock][TargetIndex] = Blocks.size() - 1;
      } else if (!isTerminator(Instr)) {
        // A new IeleCFGBlock is needed because of a conditional branch in the middle of the IeleBlock.
        Blocks.push_back(IeleCFGBlock(&TargetBlock, TargetIndex, Index - 1));
        BlockMap[&TargetBlock][TargetIndex] = Blocks.size() - 1;
        TargetIndex = Index;
      } else {
        solAssert(isUnconditionalTerminator(Instr),
                  "Invalid number of operands in a branch instruction!");
        // A new IeleCFGBlock is needed because of a ret, revert, or unconditional branch
        // in the middle of the IeleBlock.
        // This is an unconditional terminator, so no need to continue in this IeleBlock.
        Blocks.push_back(IeleCFGBlock(&TargetBlock, TargetIndex, Index));
        BlockMap[&TargetBlock][TargetIndex] = Blocks.size() - 1;
        break;
      }
    }
  }

//llvm::errs() << "IeleCFGBlock list:\n";
//for (const IeleCFGBlock &CFGBlock : Blocks) {
//  llvm::errs() << "\t" << CFGBlock.getTargetBlock()->getName() << "(";
//  llvm::errs() << CFGBlock.getTargetStartIndex() << ", ";
//  llvm::errs() << CFGBlock.getTargetEndIndex() << "):\n";
//  for (const IeleInstruction *Instr : CFGBlock.instructions()) {
//    llvm::errs() << "\t\t";
//    Instr->print(llvm::errs());
//    llvm::errs() << "\n";
//  }
//}
//
//llvm::errs() << "\n\nIeleCFGBlock Map:\n";
//for (const auto &P : BlockMap) {
//  const IeleBlock *TargetBlock = P.first;
//  for (const auto &Q : P.second) {
//    unsigned TargetStartIndex = Q.first;
//    const IeleCFGBlock &CFGBlock = Blocks[Q.second];
//    llvm::errs() << "\t" << TargetBlock->getName() << ", " << TargetStartIndex << " --> ";
//    llvm::errs() << CFGBlock.getTargetBlock()->getName() << "(";
//    llvm::errs() << CFGBlock.getTargetStartIndex() << ", ";
//    llvm::errs() << CFGBlock.getTargetEndIndex() << ")\n";
//  }
//}

  // Create the CFG edges by populating the Successors and Predecessors maps.
  for (auto It = F->begin(), ItEnd = F->end(); It != ItEnd; ++It) {
    const IeleBlock *TargetBlock = &*It;
    for (const auto &Pair : BlockMap[TargetBlock]) {
      unsigned TargetStartIndex = Pair.first;
      const IeleCFGBlock &CFGBlock = Blocks[Pair.second];
      solAssert(CFGBlock.getTargetStartIndex() == TargetStartIndex,
                "Corrupt IeleCFGBlock found!");
      unsigned TargetEndIndex = CFGBlock.getTargetEndIndex();

      // Add edges due to branch instructions.
      for (auto It = CFGBlock.rbegin(), ItEnd = CFGBlock.rend(); It != ItEnd; ++It) {
        const IeleInstruction *Instr = *It;

        if (Instr->getOpcode() != IeleInstruction::Br)
          break;

        const IeleBlock *SuccBlock =
          llvm::dyn_cast<const IeleBlock>(Instr->getIeleOperandList().back());
        solAssert(SuccBlock, "Found branch instruction with no target block!");
        if (BlockMap.count(SuccBlock) && BlockMap[SuccBlock].count(0)) {
          const IeleCFGBlock &SuccCFGBlock = Blocks[BlockMap[SuccBlock][0]];
          Successors[&CFGBlock].insert(&SuccCFGBlock);
          Predecessors[&SuccCFGBlock].insert(&CFGBlock);
        }
      }

      // Add edge to the following block, in case this IeleCFGBlock
      // doesn't end with an unconditional branch, ret, or revert.
      if (TargetBlock->empty() || !isUnconditionalTerminator(CFGBlock.back())) {
        const IeleCFGBlock *FollowingCFGBlock = nullptr;
        if (TargetEndIndex + 1 < TargetBlock->size()) {
          if (BlockMap.count(TargetBlock) && BlockMap[TargetBlock].count(TargetEndIndex + 1)) {
            FollowingCFGBlock = &Blocks[BlockMap[TargetBlock][TargetEndIndex + 1]];
          }
        } else {
          auto ItNext = next(It);
          if (ItNext != ItEnd) {
            const IeleBlock *FollowingBlock = &*ItNext;
            if (BlockMap.count(FollowingBlock) && BlockMap[FollowingBlock].count(0)) {
              FollowingCFGBlock = &Blocks[BlockMap[FollowingBlock][0]];
            }
          }
        }
        if (FollowingCFGBlock) {
          Successors[&CFGBlock].insert(FollowingCFGBlock);
          Predecessors[FollowingCFGBlock].insert(&CFGBlock);
        }
      }
    }
  }
}

void IeleCFG::print(llvm::raw_ostream &OS, unsigned indent) {
  std::string Indent(indent, ' ');
  OS << Indent << "CFG for function: " << Function->getName() << ":\n";
  for (const IeleCFGBlock &Block : blocks()) {
    OS << Indent << "  Block " << Block.getName() << "\n";
    OS << Indent << "  ->successors: ";
    bool first = true;
    for (const IeleCFGBlock *SuccBlock : successors(&Block)) {
      if (!first) OS << ", ";
      OS << SuccBlock->getName();
      first = false;
    }
    OS << "\n" << Indent << "  <-predecessors: ";
    first = true;
    for (const IeleCFGBlock *PredBlock : predecessors(&Block)) {
      if (!first) OS << ", ";
      OS << PredBlock->getName();
      first = false;
    }
    OS << "\n";
  }
  OS << "\n";
}
