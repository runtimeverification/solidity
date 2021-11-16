# The Solidity to IELE Compiler

This is the Solidity to IELE compiler, a fork of the [Solidity compiler](https://github.com/ethereum/solidity) targeting the [IELE](https://github.com/runtimeverification/iele-semantics) virtual machine (instead of the EVM).

To learn more about the supported Solidity features and the main differences between the IELE and EVM compilers, see [README-IELE-SUPPORT](README-IELE-SUPPORT.md). 

Solidity is a statically typed, contract-oriented, high-level language for implementing smart contracts on the Ethereum platform.

## Building

### Prerequisites

* [install IELE](https://github.com/runtimeverification/iele-semantics/blob/master/INSTALL.md).

### Dependencies

For MacOS:

```
brew update
brew install boost
brew install cmake
brew install llvm@11
brew install libxml2
brew upgrade
```

For Ubuntu-based systems:

```
sudo apt-get update
sudo apt-get install build-essential cmake g++ gcc git libboost-all-dev unzip llvm-11 zlib1g-dev libz3-dev libxml2-utils
```

Please note that we have tested the compiler for the Ubuntu 20.04 and MacOS BigSur operation system.

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

## Testing

To run the execution test suite, first start an IELE vm in separate terminal (this assumes the `kiele` exexcutable is found in the system's PATH):
```
kiele vm --port 9001
```

Then, start the `iele-test-client`executable in a separate terminal:
```
iele-test-client <path to Solidity-to-IELE build directory>/ipcfile 9001
```
Note that, there is no need for an exisiting `ipcfile`, this command will create a fresh one. You may need to delete an existing `ipcfile` before you restart this script.

Also note that the `iele-test-client` executable can be found in ``"$(dirname `which kiele`)/../lib/kiele"``, assuming the `kiele` executable is in the system's PATH.

Finally, you can run the whole execution test suite with:
```
./build/test/soltest --no_result_code \
                     --report_sink=build/report.xml \
                     --report_level=detailed \
                     --report_format=XML \
                     --log_sink=build/log.xml \
                     --log_level=all \
                     --log_format=XML  \
                     `cat test/failing-exec-tests` \
                     `cat test/out-of-scope-exec-tests` \
                     `cat test/unimplemented-features-tests` \
                     -- \
                     --enforce-no-yul-ewasm \
                     --ipcpath build/ipcfile \
                     --testpath test

xmllint --xpath "//TestCase[@assertions_failed!=@expected_failures]" build/report.xml && false
```

The output of the last command should look like this for a successful run of the test suite:

```
XPath set is empty
```

Alternatively, you can run a specific test with:
```
./build/test/soltest --no_result_code -t <test name> -- --enforce-no-yul-ewasm --ipcpath build/ipcfile --testpath test
```
where `<test name>` is the name with which the test is registered in the test suite. For example, the test found in `test/libsolidity/semanticTests/operators/shifts/shift_right.sol`, is registered in the test suite with the name `semanticTests/operators/shifts/shift_right`.
