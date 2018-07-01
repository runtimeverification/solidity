# `uint` and `uint256`

A `uint` value cannot be coerced into a `uint256` value. For example,
the IELE-Solidity will reject the following code, even though the
EVM-Solidity compiler would not:


    function caller(uint u) internal {
      called(u);
    }
    
    function called(uint256 u) internal {}
    
It will also reject the case where a `uint` return variable is
assigned to a `uint256` local.

There is an exception to this rule. Constant values (like `0x3`) are
stored as `uint`. If they're small enough to fit into a `uint256`,
they can be coerced into one. Here's an example that takes a `uint256`
as an argument:

    
    function caller() internal {
      called(0x3);
    }
    
    function called(uint256 u) ... 

TODO: Can they just replace all uint256s with uints?

