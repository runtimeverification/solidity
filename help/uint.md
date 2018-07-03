# `uint` and `uint256`

(The same ideas apply to `int` and `int256`.)

A `uint` can be *explicitly* cast into a `uint256` (or smaller)
variable:
    
    function called(uint u) internal {
      uint256 smaller = uint256(u);
    }
    

If the actual value is too long to fit, it will be truncated.
(This is the same behavior as in EVM-Solidity.)

However, a `uint` value will not be *implicitly* coerced into a
`uint256` value. For example, the IELE-Solidity compiler will reject
the following code, even though the EVM-Solidity compiler would not:


    function caller(uint u) internal {
                    ^^^^
      called(u);
    }
    
    function called(uint256 u) internal {}
                    ^^^^^^^
    
IELE-Solidity will also reject the case where a `uint` return value is
assigned to a `uint256` local.

There is an exception to this rule. Constant values (like `0x3`) are
of type `uint`. But if they're small enough to fit into a `uint256`,
they can be coerced into one. This variant of `caller` compiles:

    
    function caller() internal {
      called(0x3);
    }
    
    function called(uint256 u) internal {}
    
Notes: 

1. `uint256` isn't a special case. The above would still work if the
   `called` parameter were declared as a `uint8`.

2. The compiler doesn't track sizes through assignments. The
   following, for example, will produce a compile error:

        function caller() internal {
          uint u = 0x3;
          called(u);
        }

