# Installing the Solidity to IELE compiler

## Prerequisites

* [install IELE](https://github.com/runtimeverification/iele-semantics/blob/master/INSTALL.md).

## Dependencies
To easily install the required dependencies, run 
```
sudo ./scripts/install_deps.sh
```

## Build

```
mkdir build
cd build
cmake ..
make
```

## Usage

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
