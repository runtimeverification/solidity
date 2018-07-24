# Error: `address.delegatecall` is not supported in IELE

*Note: the error message is different in the current compiler.*

## Quick look

Old: 
    
    contract LibraryContract {
      function libraryFunction() public;  // an abstract function and contract
    }
      
    contract Contract {
      ...
      ... LibraryContract(0x3bBb367Afe5075E0461F535d6Ed2A640822EDb1C) ...
      ...
    }

New:
    
    import "/path/to/LibraryContract.sol"

    contract Contract is LibraryContract {
      function contractFunction() public {
        libraryFunction();                    // <<<-
      }
    }
   
   
## Discussion

The compiler needs the library's source, typically via an
`import`. After that, the library contract is accessed via multiple
inheritance:


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

