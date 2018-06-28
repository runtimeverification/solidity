# Support for moving Solidity code from EVM to IELE

## A checklist

* **Consider the difference between `uint256` and `uint`.**
  
  [TBD]

* **Run the compiler and look for errors.**

  The compiler will tell you of remaining changes that need to be
  made.  If you see errors, look for them in the following section.

## Error messages

Some code that the Ethereum compiler allows is invalid in this
compiler, usually for security reasons. This section links error
messages to advice about how to make them go away.

* [Low-level calls are not supported in IELE](call.md)
* [`msg.data` is not supported in IELE.](msg-data.md)


