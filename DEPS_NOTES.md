# Working 
* Ubuntu
* Arch Linux
* Debian 
* Fedora

# Not working 
* Darwin
* CentOS
* Alpine Linux

# Notes

## Alpine

Tried both 3.6 and 3.7 (currenly latest)

### Alpine 3.6

* `apk is OLD` error message
   * fix with `sudo apk add --upgrade apk-tools@edge` (added to script)
   * see https://wiki.alpinelinux.org/wiki/Alpine_Linux_package_management#.22apk-tools_is_old.22
* only `llvm4` is available in packet manager (still worth trying) 
* `Cmake` error
   * Requires manually changing `llvm` to `llvm4` in `/usr/lib/cmake/llvm5/LLVMConfig.cmake`
* After that, still have several compilation issues 
   * `/home/vagrant/solidity/libiele/IeleValue.h:3:34: fatal error: llvm/Support/Casting.h: No such file or directory
 #include "llvm/Support/Casting.h"`

### Alpine 3.7

* `lvm5` available here
* Still requires manually changing `llvm` to `llvm5` in `/usr/lib/cmake/llvm5/LLVMConfig.cmake` for `Cmake` to succeed
* Cmake still fails  
    * ```CMake Error at /usr/lib/cmake/llvm5/LLVMExports.cmake:975 (message):
         The imported target "LLVMDemangle" references the file "/usr/lib/llvm5/lib/libLLVMDemangle.a"
         but this file does not exist. Possible reasons include... ``` (broken package etc.) 
    * Looked for it in file system but does not exist
    * No info online
