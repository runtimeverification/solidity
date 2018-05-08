# Working 

* Darwin (needs manual intervention, see below)
* Ubuntu
* Arch Linux
* Debian 
* Fedora
* Alpine Linux (only 3.6, with manual intervention)

# Not working 
* CentOS

# Caveats

## Alpine Linux 3.6

Alpine 3.6 currently comes with `llvm4`. 

When running `cmake`, you may get the following error:

```
CMake Error at /usr/lib/cmake/llvm4/LLVMConfig.cmake:182 (include):
  include could not find load file:

    /usr/lib/cmake/llvm/LLVMExports.cmake
Call Stack (most recent call first):
  cmake/EthDependencies.cmake:47 (find_package)
  CMakeLists.txt:22 (include)
```

In order to fix it: 

* open `/usr/lib/cmake/llvm4/LLVMConfig.cmake` (may need to use `sudo`)
* look for a line that looks like `set(LLVM_CMAKE_DIR "${LLVM_INSTALL_PREFIX}/lib/cmake/llvm")`    
* change the path so that it uses `llvm4` i.e. `set(LLVM_CMAKE_DIR "${LLVM_INSTALL_PREFIX}/lib/cmake/llvm4")` 

If you now run `cmake` again, it should work just fine. 

## Darwin

The standard installation steps apply, with this single exception:

* Use `cmake -DCMAKE_PREFIX_PATH=/usr/local/opt/llvm ..`
    * The path to be used is obtained with `$(brew --prefix llvm)`
    * Also see: https://embeddedartistry.com/blog/2017/2/20/installing-clangllvm-on-osx

# Notes about not-working platforms:

## Alpine 3.7

* `llvm5` is actually available here (while 3.6 only has llvm4)
* Still requires manually changing `llvm` to `llvm5` in `/usr/lib/cmake/llvm5/LLVMConfig.cmake` for `Cmake` to succeed
* Cmake still fails  
    * ```CMake Error at /usr/lib/cmake/llvm5/LLVMExports.cmake:975 (message):
         The imported target "LLVMDemangle" references the file "/usr/lib/llvm5/lib/libLLVMDemangle.a"
         but this file does not exist. Possible reasons include... ``` (broken package etc.) 
    * Looked for it in file system but does not exist
    * No info online
