## Installing IELE Backend for the Solidity Compiler

These instructions will help you to install the solidity compiler with the IELE backend in an Ubuntu 16.04 machine:

First step is to [install IELE](https://github.com/runtimeverification/iele-semantics/blob/master/INSTALL.md).

```
# install dependencies
sudo ./scripts/install_deps.sh

# build compiler, it is important to build into a subdir named "build" as
# suggested in order to run the test scripts correctly
mkdir build
cd build
cmake ..
make
```

Use the compiler like this:
```
./build/solc/solc --asm <solidity file>
```

To run the compilation tests:
```
./test/ieleCmdlineTests.sh

# failed tests reported are stored in test/failed, clean them before rerunning
rm -rf test/failed
```
