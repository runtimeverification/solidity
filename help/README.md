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
  
  Otherwise, you'll be safe changing all `uint256` variables to `uint`
  and all `int256` variables to `int`. You could also leave them as
  they are. In that case, see [uint.md](uint.md).
  
* **Check casts of `int` to `uint`**

  In EVM-Solidity, you can cast any integer type into `uint`, so
  the following is valid:
  
      function converter(int8 short) { 
        uint long = uint(short);
        ...
      }
  
  Negative values will become positive ones. In the above case, `-1`
  will become `255`. If the input were an `int` rather than an `int8`,
  it would become a very large number indeed.
    
  In IELE-Solidity, converting a negative value to a `uint` causes a
  runtime exception. If you want to preserve EVM-Solidity behavior,
  search for casts to `uint` and add the code highlighted here:
    
      function converter(int8 short) { 
        uint long = uint(uint8(short));
                         ^^^^^
      }

* **Change uses of `call` and `msg.data`**

  IELE has a different mechanism for low-level calls, and the compiler
  doesn't accept `address.call` or support `msg.data`.  See [call.md](call.md).

* **Provide source for libraries you use**

  There are two forms of library in Solidity: contracts that are
  invoked using an explicit `delegatecall` (formerly `callcode`), and
  the
  [library](http://solidity.readthedocs.io/en/v0.4.24/contracts.html#libraries)
  construct, which hides the `delegatecall` from you. In both cases,
  you're calling someone else's bytecodes *and* (unlike with `call`)
  giving them access to your context. That's a security risk.
  
  Therefore, IELE doesn't allow `delegatecall`. Instead, the library's
  source is compiled and the resulting bytecodes are included in your compiled
  contract. (That means you're paying for increased security with the
  slightly higher gas price for bigger contracts.)
  
  **Libraries**
  
  The case for libraries defined with `library` is simplest.  All you
  need to do is give the library's source to the compiler (with an
  `import`).
  
  **Library contracts**
  
  Library contracts are more awkward. See
  [`delegatecall.md`](delegatecall.md).

* **Check uses of `ecrecover`**
  
  The EVM-Solidity version of `ecrecover` returns `0` in case of
  error. Code that doesn't check for the error case might incorrectly
  and irreveribly send funds to address `0`.
  
  Therefore, the IELE-Solidity version throws an exception instead. So
  if you *did* check the return value, you can now remove that check.

* **Run the compiler and look for errors.**

  The compiler will tell you of remaining changes that need to be
  made. The following section shows you what to do in response to 
  various error messages. It includes the error messages you'll get
  if you missed some low-level calls. 

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
  
  In EVM-Solidity, `msg.data` is used to receive arguments given to low-level
  calls (`call` and `delegatecall`). Since those calls aren't supported
  in IELE, `msg.data` isn't either. See [call.md](call.md).

