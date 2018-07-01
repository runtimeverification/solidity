# Moving Solidity code from EVM to IELE

It would be simplest if the Solidity compiled to IELE bytecodes were
exactly the same as the one compiled to EVM. There are some differences, though, for reasons of security or reliability. This page will help you convert existing contracts. 

## A checklist

* **Consider the difference between `uint256` and `uint`.**
  
  In EVM-Solidity, `uint` is just shorthand for `uint256`. 
  In IELE-Solidity, `uint` is an integer that grows as long
  as is needed (so there can't be overflow).
  
  [uint.md](uint.md) discusses some of the consequences for
  your code. 
  
* **TODO**: ecrecover

* **Run the compiler and look for errors.**

  The compiler will tell you of remaining changes that need to be
  made. The following section shows you how to fix various error
  messages.

## Error messages

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

* **Inline assembly is not supported in IELE.**

  There are Solidity-level builtins for what
  used to require assembly language. See 
  [assembly.md](assembly.md). 

* **`msg.sig` is not supported in IELE.**
  
  There's no way to fetch the function signature part of the `msg`.
  In IELE, the name a function was called under is always that
  function's name.
