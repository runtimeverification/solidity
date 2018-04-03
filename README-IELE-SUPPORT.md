## Solidity features that we DO NOT plan to support

A few Solidity features will not be supported by the IELE backend due to one or more of the following reasons:

* Deprecated feature (DF)
* Security concerns (SC)
* Not expressible in IELE (NE)

The Solidity features that will not be supported are listed below along with the reasons of not support, given as letter codes.

| Feature | Link | Reasons |
|---------|------|---------|
| `call` member of the Address type | [doc link](https://solidity.readthedocs.io/en/v0.4.20/units-and-global-variables.html#address-related) | SC NE |
| `delegetcall` member of the Address type | [doc link](https://solidity.readthedocs.io/en/v0.4.20/units-and-global-variables.html#address-related) | SC NE |
| `callcode` member of the Address type | [doc link](https://solidity.readthedocs.io/en/v0.4.20/units-and-global-variables.html#address-related) | DF SC NE |
| `selector` member of the Function type | [doc link](https://solidity.readthedocs.io/en/v0.4.20/types.html#function-types) | NE |
| `msg.sig`  built-in function for querying calldata | [doc link](https://solidity.readthedocs.io/en/v0.4.20/units-and-global-variables.html#block-and-transaction-properties) | NE |
| `msg.data` built-in function for querying calldata | [doc link](https://solidity.readthedocs.io/en/v0.4.20/units-and-global-variables.html#block-and-transaction-properties) | NE |
| Inline EVM assembly | [doc link](https://solidity.readthedocs.io/en/v0.4.20/assembly.html#solidity-assembly) | SC NE |

Finally, it is planned that the `uint` and `int` Solidity types will be changed from syntactic sugar for `uint256` and `int256` respectively to unbounded integer types. As a consequence of that, we also do not guarantee that we will follow the documented [storage](https://solidity.readthedocs.io/en/v0.4.20/miscellaneous.html#layout-of-state-variables-in-storage) and [memory](https://solidity.readthedocs.io/en/v0.4.20/miscellaneous.html#layout-in-memory) layouts, since we have to accommodate these new unbounded types.

## Differences in behavior between Solidity-to-EVM and Solidity-to-IELE

Because security flaws in the design of EVM led to some design changes being made between EVM and IELE, several minor differences exist
between the behavior of a contract when compiled to Solidity versus compiled to IELE. These changes were made in order to improve the
ease with which contracts can be written to be functionally correct and free of bugs.

* The `uint` and `int` types, having become arbitrary precision integers, no longer overflow at 2^256. Furthermore, because of this change, if a negative value of integer type is cast to the `uint` type, an exception is thrown
  rather than attempting to interpret its value modulo some fixed bit width.

* The `ecrecover` function no longer returns zero on failure, as this can lead improperly written code to send funds to address zero, locking them permanently. Instead, the underlying iele primitive returns the exceptional value -1, and Solidity invocations of the higher level built-in function will check for this error case and throw an exception.
