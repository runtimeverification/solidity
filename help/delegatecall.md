# Error: `address.delegatecall` is not supported in IELE

*Note: the error message is different in the current compiler.*

## Quick look

Old: 
    
    contract Contract {
      address lib;
    
      constructor() public {
        lib = new LibraryContract();
      }
    
      function contractFunction() public {
        lib.delegatecall(bytes4(keccak256("libraryFunction()")));
      }
    }

New:

    contract Contract is LibraryContract {  <<<- Note the inheritance
      function contractFunction() public {
        libraryFunction();
      }
    }
   
   
## Discussion

In EVM-Solidity, library contracts are often used by calling them with
`delegatecall`, which transfers the calling context an instance of a 
contract on the blockchain.

Since that isn't allowed in IELE-Solidity, a library function is
accessed using multiple inheritance:

    contract Contract is LibraryContract {

Within the contract, library functions are called in the normal way:

    contract Contract is LibraryContract {
      function contractFunction() public {
        libraryFunction();
      }
    }
    

## Examples

* Before: [examples/delegatecall.evm.sol](examples/delegatecall.evm.sol)
* After: [examples/delegatecall.iele.sol](examples/delegatecall.iele.sol)

