# Support for moving Solidity code from EVM to IELE

## A checklist

* **Consider the difference between `uint256` and `uint`.**
  
  [TBD]
  
* **TODO**: ecrecover

* **Run the compiler and look for errors.**

  The compiler will tell you of remaining changes that need to be
  made.  If you see errors, look for them in the following section.

## Error messages

Some code that the Ethereum compiler allows is invalid in this
compiler, usually for security reasons. This section links error
messages to advice about how to make them go away.

* [`address.call` is not supported in IELE](call.md)

* [`address.delegatecall` is not supported in IELE](delegatecall.md)
  
  The solution given also applies to `address.callcode`.
  
* [Inline assembly is not supported in IELE.](assembly.md)

* Member "selector" is not supported in IELE.

  Constructs like `this.myFunction.selector` are no longer
  needed for low-level calls. See [`call.md`](call.md) for the
  IELE mechanism that replaces `call`, `delegatecall` and
  `callcode`. 

* [`msg.data` is not supported in IELE.](msg-data.md)

* `msg.sig` is not supported in IELE.
  
  There's no way to fetch the function signature part of the `msg`.
  In IELE, the name a function was called under is always that
  function's name.

* **TODO**: abi.encodeWithSignature, abi.encodeWithSelector
