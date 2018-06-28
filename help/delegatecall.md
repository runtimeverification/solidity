# Error: Low-level calls are not supported in IELE

## Quick look

Old: 
    
    contract Contract {
      address lib;
    
      constructor() {
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
`delegatecall`, which transfers the calling context to the library contract. For example, in this library code:

    contract LibraryContract {
      event libraryEvent(address from);
  
      function libraryFunction() public {
        emit libraryEvent(this);
      }
    }

... you don't want the `libraryEvent` to carry the address of
`LibraryContract`, but rather that of the client contract that's using
`LibraryContract`.

`delegatecall` requires some verbose mechanism to set up, such as this:

      constructor() {
        lib = new LibraryContract();
      }
    
      function contractFunction() public {
        lib.delegatecall(bytes4(keccak256("libraryFunction()")));
      }

In IELE, such a client instead inherits from the library:

    contract Contract is LibraryContract {

The library code is copied into the client contract, so it can be
called like any other internal function:

    contract Contract is LibraryContract {
      function contractFunction() public {
        libraryFunction();
      }
    }

**Question: is there anything to be said about library function arguments?**

**Todo? Put the `uint` vs. `uint256` issue onto its own page?**


## Examples

* Before: [examples/delegatecall.evm.sol](examples/delegatecall.evm.sol)
* After: [examples/delegatecall.iele.sol](examples/delegatecall.iele.sol)

