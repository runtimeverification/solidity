# The Solidity to IELE Compiler

This is the Solidity to IELE compiler, a fork of the [Solidity compiler](https://github.com/ethereum/solidity) targeting the [IELE](https://github.com/runtimeverification/iele-semantics) virtual machine (instead of the EVM). 

## Useful links
To get started you can find an introduction to the language in the [Solidity documentation](https://solidity.readthedocs.org). In the documentation, you can find [code examples](https://solidity.readthedocs.io/en/latest/solidity-by-example.html) as well as [a reference](https://solidity.readthedocs.io/en/latest/solidity-in-depth.html) of the syntax and details on how to write smart contracts.

You can start using [Solidity in your browser](http://remix.ethereum.org) with no need to download or compile anything.

The changelog for this project can be found [here](https://github.com/ethereum/solidity/blob/develop/Changelog.md).

Solidity is still under development. So please do not hesitate and open an [issue in GitHub](https://github.com/ethereum/solidity/issues) if you encounter anything strange.

## Building

### Prerequisites

* [install IELE](https://github.com/runtimeverification/iele-semantics/blob/master/INSTALL.md).

### Dependencies

To easily install the required dependencies, run 

```
sudo ./scripts/install_deps.sh
```

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

Failed tests reported are stored in test/failed, clean them before rerunning:


```
rm -rf test/failed
```

The standard [Solidity documentation](https://solidity.readthedocs.io/en/latest/installing-solidity.html#building-from-source) contains build instructions relevant for both EVM and IELE backends.