# The Solidity to IELE Compiler

This is the Solidity to IELE compiler, a fork of the [Solidity compiler](https://github.com/ethereum/solidity) targeting the [IELE](https://github.com/runtimeverification/iele-semantics) virtual machine (instead of the EVM).

To learn more about the supported Solidity features and the main differences between the IELE and EVM compilers, see [README-IELE-SUPPORT](README-IELE-SUPPORT.md). 

Solidity is a statically typed, contract-oriented, high-level language for implementing smart contracts on the Ethereum platform.

## Table of Contents

- [Background](#background)
- [Build and Install](#build-and-install)
- [Example](#example)
- [Documentation](#documentation)
- [Development](#development)
- [Maintainers](#maintainers)
- [License](#license)
- [Security](#security)

## Background

## Building

### Prerequisites

* [install IELE](https://github.com/runtimeverification/iele-semantics/blob/master/INSTALL.md).

### Dependencies

To easily install the required dependencies on your system, run 

```
sudo ./scripts/install_deps.sh
```

We have successfully tested the script on the following operating systems:

* Darwin
* Ubuntu
* Arch Linux
* Debian 
* Fedora
* Alpine Linux

### Build the compiler

```
mkdir build
cd build
cmake ..
make
```

## Usage

Use the compiler like this:

```
./build/solc/isolc --asm <solidity file>
```

To run the compilation tests:
```
./test/ieleCmdlineTests.sh
```

Failed tests reported are stored in `test/failed`, clean them before rerunning:

```
rm -rf test/failed
```
