set(sources
    IeleArgument.cpp
    IeleArgument.h
    IeleBlock.cpp
    IeleBlock.h
    IeleConstant.cpp
    IeleConstant.h
    IeleContext.cpp
    IeleContext.h
    IeleContract.cpp
    IeleContract.h
    IeleFunction.cpp
    IeleFunction.h
    IeleGlobalValue.cpp
    IeleGlobalValue.h
    IeleGlobalVariable.cpp
    IeleGlobalVariable.h
    IeleInstruction.cpp
    IeleInstruction.def
    IeleInstruction.h
    IeleIntConstant.cpp
    IeleIntConstant.h
    IeleLocalVariable.cpp
    IeleLocalVariable.h
    iele-rt/iele-memory-rt.h
    iele-rt/iele-storage-rt.h
    IeleValue.cpp
    IeleValue.def
    IeleValue.h
    IeleValueSymbolTable.cpp
    IeleValueSymbolTable.h
    SymbolTableListTraits.h
    transforms/DeadCodeElimination.cpp
    transforms/IeleCFG.h
    transforms/IeleCFG.cpp
    transforms/IeleTransforms.h
)

add_library(iele ${sources})
target_link_libraries(iele PUBLIC solutil)
target_include_directories(iele PUBLIC ${LLVM_INCLUDE_DIRS})
