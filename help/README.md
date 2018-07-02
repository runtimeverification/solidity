# Moving Solidity code from EVM to IELE

It would be simplest if using IELE required no changes to your
Solidity contracts. Some contracts do require changes, though, for
reasons of security or reliability. This page will help you convert
them.

## A checklist

* **Consider the difference between `uint256` and `uint`.**
  
  In EVM-Solidity, `uint` is just shorthand for `uint256`. 
  In IELE-Solidity, `uint` is an integer that grows as long
  as is needed (so there can't be overflow).
  
  Similarly, in IELE-Solidity, `int` is now arbitrarily long.
  
  In some rare cases, you may have written a contract where you
  *want* an integer to overflow. If so, you'll need to find cases
  where the integer is defined as `uint` or `int` and change them
  to `uint256` or `int256`. 
  
  Otherwise, you'd be safe changing all `uint256` variables to `uint`
  and all `int256` variables to `int`. You could also leave them as
  they are. In that case, see [uint.md](uint.md).
  
* In EVM-Solidity, you can cast any `int` type into `uint`, so
  the following is valid:
  
      function converter(int8 short) { 
        uint long = uint(short);
        ...
      }
  
  ... even though, because of how fixed-length negative numbers are
  stored, a small negative number will become a very large positive
  number.
    
  In IELE-Solidity, converting a negative `intN` value to a `uint`
  causes an exception. If you want to preserve EVM-Solidity behavior,
  search for casts (`uint(`), then (1) cast the fixed-width `intN` to a
  `uintN`, and (2) cast the result to `uint`:
    
      function converter(int8 short) { 
        uint long = uint(uint8(short));
                         ^^^^^
      }

* **Change uses of `call`**

  IELE has a different mechanism for low-level calls, and the compiler
  doesn't accept `call`.  See [call.md](call.md).

* **Provide source for the libraries you use**

  There are two forms of library in Solidity: contracts that are
  invoked using an explicit `delegatecall` (formerly `callcode`), and
  the
  [library](http://solidity.readthedocs.io/en/v0.4.24/contracts.html#libraries)
  construct, which hides the `delegatecall` from you. In both case,
  the bytecodes for the library are stored once in the blockchain,
  separately from your contract.

  For security, IELE doesn't allow `delegatecall`. Instead, library
  source is compiled and the bytecodes are included in your compiled
  contract. (That means you're paying for increased security with the
  slightly higher gas price of bigger contracts.)

  The consequence to you is that you cannot have code like this:
  
      contract LibraryContract {
        function libraryFunction() public;  // an abstract function and contract
      }
      
      contract Contract {
        ...
        ... LibraryContract(0x3bBb367Afe5075E0461F535d6Ed2A640822EDb1C) ...
        ...
      }

  That code cannot be compiled until you tell the compiler where
  the source for `LibraryContract` is (by providing it inline
  or via `import`) The same must be done for the `library` construct.
  
  After doing it for a `library`, you needn't make any other changes
  to your source. If you have an explicit `delegatecall`, you use
  inheritance. See 
  [`delegatecall.md`](delegatecall.md).

* **Change uses of `msg.data`**

  In EVM-Solidity, `msg.data` was used to receive arguments given to low-level
  calls. [msg-data.md](msg-data.md) describes how that's done in
  IELE-Solidity.

* **Check uses of `ecrecover`**
  
  The `ecrecover` built-in function now throws an exception on
  failure, instead of returning `0`. The reason is that code that
  didn't check the return value might then send funds to address `0` --
  and those funds could never be gotten back.
  
  If you *did* check the return value, you can now remove that check.

* **Run the compiler and look for errors.**

  The compiler will tell you of remaining changes that need to be
  made. The following section shows you what to do in response to 
  various error messages. It includes the error messages for any
  low-level calls you might have overlooked.

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

