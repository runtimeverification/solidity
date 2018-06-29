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

      constructor() public {
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
    
### If library source is unavailable

**TODO** I suspect the following is wrong about construction time.

If the source is unavailable at compile time, it will be copied from
the library when a contract is constructed. Some information is still
needed at compile time, and that can be supplied with an abstract
contract matching the real contract. 

**Question: is there anything to be said about library function arguments?**

**Todo? Put the `uint` vs. `uint256` issue onto its own page?**


## Examples

* Before: [examples/delegatecall.evm.sol](examples/delegatecall.evm.sol)
* After: [examples/delegatecall.iele.sol](examples/delegatecall.iele.sol)

