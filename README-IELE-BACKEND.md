## Installing IELE Backend for the Solidity Compiler

These instructions will help you to install the solidity compiler with the IELE backend in an Ubuntu 16.04 machine:

```
# install dependencies
sudo apt-get update
sudo apt-get make gcc cmake build-essential libboost-all-dev llvm-5.0 libz3-dev zlib1g-dev

# build compiler, it is important to build into a subdir named "build" as
# suggested in order to run the test scripts correctly
mkdir build
cd build
cmake ..
make
```

Use the compiler like this:
```
./build/solc/solc --iele <solidity file>
```

To run the compilation tests:
```
./test/ieleCmdlineTests.sh

# failed tests reported are stored in test/failed, clean them before rerunning
rm -rf test/failed
```

