#include "IeleCFG.h"
#include "IeleTransforms.h"

#include "libiele/IeleContract.h"

#include "llvm/Support/raw_ostream.h"

using namespace std;
using namespace solidity;
using namespace solidity::iele;
using namespace solidity::iele::transform;


using live_variables_set_t = unordered_set<const IeleLocalVariable *>;
using live_variables_map_t = map<const IeleInstruction *, live_variables_set_t>;

static void liveVariablesTransferOverInstruction(
  const IeleInstruction *Instr,
  const live_variables_set_t &LiveAfter,
  live_variables_set_t &LiveBefore) {

  // LiveBefore = Uses U (LiveAfter - Defs)
  LiveBefore = LiveAfter;
  for (const IeleLocalVariable *Def : Instr->lvalues())
    LiveBefore.erase(Def);
  for (const IeleValue *Operand : Instr->operands()) {
    if (const IeleLocalVariable *Use = llvm::dyn_cast<const IeleLocalVariable>(Operand)) {
      LiveBefore.insert(Use);
    }
  }
}

static void liveVariablesTransferOverBlock(
  const IeleCFGBlock &Block,
  const live_variables_set_t &LiveAfter,
  live_variables_set_t &LiveBefore) {

  live_variables_set_t Live = LiveAfter;
  for (auto It = Block.rbegin(), ItEnd = Block.rend(); It != ItEnd; ++It) {
    const IeleInstruction *Instr = *It;
    liveVariablesTransferOverInstruction(Instr, Live, Live);
  }
  LiveBefore = Live;
}

static void liveVariablesAnalysis(const IeleFunction &Function,
                                  live_variables_map_t &LiveBefore,
                                  live_variables_map_t &LiveAfter) {
  // CFG
  IeleCFG *CFG = IeleCFG::Create(&Function);
  //CFG->print(llvm::errs());

  // IN and OUT sets for each basic block
  unordered_map<const IeleCFGBlock *, live_variables_set_t> In, Out;

  // Initialization
  for (const IeleCFGBlock &Block : CFG->blocks()) {
    In[&Block];  // init to empty set
    Out[&Block]; // init to empty set
  }

  // Backwards propagation, loop until no changes in IN sets.
  bool InChanged;
  do {
    InChanged = false;
    for (const IeleCFGBlock &Block : CFG->blocks()) {
      // OUT[Block] = Union{IN[SuccessorBlocks]}
      for (const IeleCFGBlock *Succ : CFG->successors(&Block)) {
        Out[&Block].insert(In[Succ].begin(), In[Succ].end());
      }
      // IN[Block] = transferFunction(OUT[Block])
      live_variables_set_t NewIn;
      liveVariablesTransferOverBlock(Block, Out[&Block], NewIn);
      if (NewIn != In[&Block]) {
        In[&Block] = NewIn;
        InChanged = true;
      }
    }
  } while (InChanged);

  // Compute live variables before and after each instruction using the final OUT
  // and propagating backwards.
  for (const IeleCFGBlock &Block : CFG->blocks()) {
    const IeleInstruction *PrevInstr = nullptr;
    for (auto It = Block.rbegin(), ItEnd = Block.rend(); It != ItEnd; ++It) {
      const IeleInstruction *Instr = *It;
      if (PrevInstr) {
        LiveAfter[Instr] = LiveBefore[PrevInstr];
      } else {
        LiveAfter[Instr] = Out[&Block];
      }
      liveVariablesTransferOverInstruction(Instr, LiveAfter[Instr], LiveBefore[Instr]);
      PrevInstr = Instr;
    }
  }
}

static bool copyPropagation(IeleBlock &B) {
  bool changed = false;
  // itarate over instructions to find copy instructions
  for (auto It = B.begin(), ItEnd = B.end(); It != ItEnd; ) {
    IeleInstruction &I = *It;
    //llvm::errs() << "Checking instruction:\n";
    //I.print(llvm::errs());
    //llvm::errs() << "\n";

    // skip non-copy instructions
    if (I.getOpcode() != IeleInstruction::Assign) {
      ++It;
      continue;
    }

    // get LHS and RHS of copy instruction
    IeleLocalVariable *LHS = *I.lvalue_begin();
    IeleValue *RHS = *I.begin();
    //if (!llvm::isa<IeleLocalVariable>(RHS) && !llvm::isa<IeleIntConstant>(RHS)) {
    if (!llvm::isa<IeleLocalVariable>(RHS)) {
      ++It;
      continue;
    }

    // If LHS == RHS, we just delete the instruction
    if (LHS == RHS) {
      //llvm::errs() << "About to erase instruction: ";
      //I.print(llvm::errs());
      //llvm::errs() << "\n";
      It = I.eraseFromParent();
      //llvm::errs() << "Erased\n";
      changed = true;
      continue;
    }

    // iterate over instructions following the copy instruction
    for (auto ItNext = next(It); ItNext != ItEnd; ++ItNext) {
      IeleInstruction &Inext = *ItNext;
      //llvm::errs() << "Target instruction:\n";
      //Inext.print(llvm::errs());
      //llvm::errs() << "\n";

      // propagate the copy in instruction's uses
      for (unsigned i = 0; i < Inext.size(); ++i) {
        if (Inext.getOperand(i) == LHS) {
          //llvm::errs() << "Replacing ";
          //Inext.getOperand(i)->print(llvm::errs());
          //llvm::errs() << " with ";
          //RHS->print(llvm::errs());
          //llvm::errs() << " in instruction:\n";
          //Inext.print(llvm::errs());
          //llvm::errs() << "\ndue to live copy instruction:\n";
          //I.print(llvm::errs());
          //llvm::errs() << "\n\n";
          Inext.setOperand(i, RHS);
          changed = true;
        }
      }

      // check if the instruction definitions kill the copy propoagation
      bool isKillingDef = false;
      for (IeleLocalVariable *Def : Inext.lvalues()) {
        if (Def == LHS || Def == RHS) {
          isKillingDef = true;
          break;
        }
      }
      if (isKillingDef)
        break;
    }

    ++It;
  }

  return changed;
}

void solidity::iele::transform::deadCodeElimination(IeleContract &C) {
  for (IeleFunction &F : C) {
    //llvm::errs() << "Function: ";
    //F.printAsValue(llvm::errs());
    //llvm::errs() << "\n";

    for (IeleBlock &B : F) {
      while (copyPropagation(B)) ;
    }


    live_variables_map_t LiveBefore;
    live_variables_map_t LiveAfter;
    liveVariablesAnalysis(F, LiveBefore, LiveAfter);

    //for (const IeleBlock &B : F) {
    //  llvm::errs() << B.getName() << ":\n";
    //  for (const IeleInstruction &I : B) {
    //    llvm::errs() << "\tLive ins:";
    //    bool first = true;
    //    for (const IeleValue *V : LiveBefore[&I]) {
    //      if (!first)
    //        llvm::errs() << ", ";
    //      V->printAsValue(llvm::errs());
    //      first = false;
    //    }
    //    llvm::errs() << "\n\t";
    //    I.print(llvm::errs());
    //    llvm::errs() << "\n";
    //    llvm::errs() << "\tLive outs:";
    //    first = true;
    //    for (const IeleValue *V : LiveAfter[&I]) {
    //      if (!first)
    //        llvm::errs() << ", ";
    //      V->printAsValue(llvm::errs());
    //      first = false;
    //    }
    //    llvm::errs() << "\n\n";
    //  }
    //}

    //llvm::errs() << "\n";

    for (IeleBlock &B : F) {
      for (auto It = B.begin(), ItEnd = B.end(); It != ItEnd; ) {
        IeleInstruction &I = *It;
        // skip call instructions
        if (I.getOpcode() >= IeleInstruction::IeleCallsBegin &&
            I.getOpcode() <= IeleInstruction::IeleCallsEnd) {
          ++It;
          continue;
        }

        // skip instructions that may have side-effects
        if (I.getOpcode() == IeleInstruction::Div ||
            I.getOpcode() == IeleInstruction::Mod ||
            I.getOpcode() == IeleInstruction::Exp ||
            I.getOpcode() == IeleInstruction::AddMod ||
            I.getOpcode() == IeleInstruction::MulMod ||
            I.getOpcode() == IeleInstruction::ExpMod ||
            I.getOpcode() == IeleInstruction::Log2 ||
            I.getOpcode() == IeleInstruction::SExt ||
            I.getOpcode() == IeleInstruction::BSwap) {
          ++It;
          continue;
        }

        // skip instructions with no lvalues
        if (I.lvalue_size() == 0) {
          ++It;
          continue;
        }

        // check if any lvalue is live immediately after the instruction
        bool lvalueAlive = false;
        for (IeleLocalVariable *LHS : I.lvalues()) {
          lvalueAlive = lvalueAlive || LiveAfter[&I].count(LHS);
        }

        // if no lvalue is live immediately after the instruction, the
        // instruction is dead and can be removed.
        if (!lvalueAlive) {
          //llvm::errs() << "About to erase instruction: ";
          //I.print(llvm::errs());
          //llvm::errs() << "\n";
          It = I.eraseFromParent();
          //llvm::errs() << "Erased\n";
        } else {
          ++It;
        }
      }
    }
  }
  return;
}
