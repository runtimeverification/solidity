# The Solidity to IELE Compiler

This is the Solidity to IELE compiler, a fork of the [Solidity compiler](https://github.com/ethereum/solidity) targeting the [IELE](https://github.com/runtimeverification/iele-semantics) virtual machine (instead of the EVM).

To learn more about the supported Solidity features and the main differences between the IELE and EVM compilers, see [README-IELE-SUPPORT](README-IELE-SUPPORT.md). 

Solidity is a statically typed, contract-oriented, high-level language for implementing smart contracts on the Ethereum platform.

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

To run the execution test suite, first start an IELE vm in separate terminal (this assumes that IELE binaries are found in the system's PATH):
```
kiele vm --port 9001
```

Then, start the `iele-test-client` script in a separate terminal:
```
iele-test-client <path to Solidity-to-IELE build directory>/ipcfile 9001
```
Note that, you may need to delete an existing `ipcfile` before you restart this script.

Finally, you can run the whole execution test suite with:
```
./build/test/soltest --no_result_code --report_level=short `cat test/failing-exec-tests` -- --enforce-no-yul-ewasm --ipcpath build/ipcfile --testpath test
```

Alternatively, you can run a specific test with:
```
./build/test/soltest --no_result_code -t <test name> -- --enforce-no-yul-ewasm --ipcpath build/ipcfile --testpath test
```
where `<test name>` is the name with which the test is registered in the test suite. For example, the test found in `test/libsolidity/semanticTests/operators/shifts/shift_right.sol`, is registered in the test suite with the name `semanticTests/operators/shifts/shift_right`.
