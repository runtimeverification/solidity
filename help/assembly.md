# Error: Inline assembly is not supported in IELE.

IELE-Solidity doesn't allow embedded assembly code. Many uses of assembly
have Solidity-level replacements.

### Elliptic curve functions

EVM's `ECADD`, `ECMUL`, and `ECPAIRING` bytecodes are only available
via assembly. In IELE, they have corresponding built-in functions:
`ecadd`, `ecmul`, and `ecpairing`.

Old: 
     
     assembly {
       let ecadd := 6 // precompiled contract
       success := call(sub(gas, 2000), ecadd, 0, temp, 0xc0, retval, 0x60)
     }
 
New:

     uint256[2] memory output = ecadd(input1, input2);


Here are the types of the three functions:
    
    function ecadd    (uint256[2],   uint256[2])   pure  returns (uint256[2])
    function ecmul    (uint256[2],   uint256)      pure  returns (uint256[2])
    function ecpairing(uint256[2][], uint256[4][])       returns (bool)
    
    
* Before: [examples/elliptic.evm.sol](examples/elliptic.evm.sol)
* After: [examples/elliptic.iele.sol](examples/elliptic.iele.sol)

### Getting the size of deployed code 

Old: 
         
     assembly { size := extcodesize(addr) }
     
New: 
     
     uint16 size = addr.codesize;
      

Note that `codesize` will still return `0` if no code has been deployed to the given address.

* Before: [examples/codesize.evm.sol](examples/codesize.evm.sol)
* After: [examples/codesize.iele.sol](examples/codesize.iele.sol)

