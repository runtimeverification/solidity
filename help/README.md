# Moving Solidity code from EVM to IELE

It would be simplest if the Solidity compiled to IELE bytecodes were
exactly the same as the one compiled to EVM. There are some
differences, though, for reasons of security or reliability. This page
will help you convert existing contracts.

## A checklist

* **Consider the difference between `uint256` and `uint`.**
  
  In EVM-Solidity, `uint` is just shorthand for `uint256`. 
  In IELE-Solidity, `uint` is an integer that grows as long
  as is needed (so there can't be overflow).
  
  Similarly, in IELE-Solidity, `int` is arbitrarily long, whereas
  `int256` is a fixed 256 bits.
  
  In most cases, you'll be safe changing all `uint256` variables to
  `uint` and all `int256` variables to `int`. The only reason not to
  is if you are actually expecting a `uint` or `int` to overflow
  (which should be rare). In that case, change the type to `uint256`
  or `int256`.
  
  If you want to have a mixture of the infinite-precision and 256-bit
  types, see [uint.md](uint.md). 
  
* In EVM-Solidity, you can cast any `int` type into `uint`, so
  the following is valid:
  
      function converter(int8 short) { 
        uint long = uint(short);
        ...
      }
  
  ... though a small negative number will become a very large
  positive number. That's an artifact of how fixed-length
  negative numbers are represented.
    
  In IELE-Solidity, converting a negative `intN` value to a `uint`
  causes an exception. If you want to preserve EVM-Solidity behavior,
  search for casts (`uint(`), then (1) cast the fixed-with `intN` to a
  `uintN`, and (2) cast the result to `uint`:
    
    function converter(int8 short) { 
      uint long = uint(uint8(short));
    }

* **Change uses of `call`**

  IELE has a different mechanism for low-level calls, and the compiler
  doesn't accept `call`.  See [`call.md`](call.md).


* **Change uses of `delegatecall` or `callcode`**

  In EVM-Solidity, these are frequently used to treat contracts as
  libraries. IELE-Solidity replaces them with a different mechanism.
  
  Important note: for security (and performance), IELE-Solidity
  requires that you have the source for any library contract.
  Suppose you have code like this:
      
      contract LibraryContract {
        function libraryFunction() public;
      }
      
      contract Contract {
        ...
        ... LibraryContract(0x3bBb367Afe5075E0461F535d6Ed2A640822EDb1C) ...
        ...
      }

  That code cannot be compiled until you convert `LibraryContract`
  from an abstract contract to a reference to its real source.
  
  Once that's been done (if required), 
  See [`delegatecall.md`](delegatecall.md) for the
  IELE-Solidity mechanism.

* **Change uses of `msg.data`**

  In EVM-Solidity, `msg.data` was used to receive low-level
  calls. [msg-data.md](msg-data.md) describes how it's done in
  IELE-Solidity.

* **Check uses of `ecrecover`**
  
  The `ecrecover` built-in function now throws an exception on
  failure, instead of returning `0`. Code that didn't check for the
  failure case could then send funds to address 0 -- and those funds
  could never be gotten back.
  
  If you *did* check for failure, you can now remove that check.

* **Run the compiler and look for errors.**

  The compiler will tell you of remaining changes that need to be
  made. The following section shows you how to fix various error
  messages.

## Error messages

* **Inline assembly is not supported in IELE.**

  There are Solidity-level builtins for what
  used to require assembly language. See 
  [assembly.md](assembly.md). 

* **`msg.sig` is not supported in IELE.**
  
  There's no way to fetch the function signature part of the `msg`.
  In IELE, the name a function was called under is always that
  function's name.

* **`address.call` is not supported in IELE**    
  **`abi.encodeWithSignature` is not supported in IELE**    
  **`abi.encodeWithSelector` is not supported in IELE**   
  **Member `selector"` is not supported in IELE.**
  
  These constructs are no longer needed for low-level calls. 
  See [`call.md`](call.md) for the IELE mechanism. 

* **`address.delegatecall` is not supported in IELE**    
  **`address.callcode` is not supported in IELE**

  In EVM-Solidity, these are frequently used to treat contracts as
  libraries. See [`delegatecall.md`](delegatecall.md) for the
  IELE-Solidity mechanism.

* **`msg.data` is not supported in IELE.**
  
  In EVM-Solidity, `msg.data` was used to receive low-level
  calls. [msg-data.md](msg-data.md) describes how it's done in
  IELE-Solidity.

