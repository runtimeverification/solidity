contract C {
    function f() public {
        (bool success,) = address(this).callcode();
        require(success);
        (success,) = address(this).callcode(uint(1));
        require(success);
        (success,) = address(this).callcode(uint(1), uint(2));
        require(success);
    }
}
// ----
// TypeError 6198: (65-87): Low-level calls are not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 6138: (65-89): Wrong argument count for function call: 0 arguments given but expected 1. This function requires a single bytes argument. Use "" as argument to provide empty calldata.
// TypeError 6198: (138-160): Low-level calls are not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 8051: (161-168): Invalid type for argument in function call. Invalid implicit conversion from uint to bytes memory requested. This function requires a single bytes argument. If all your arguments are value types, you can use abi.encode(...) to properly generate it.
// TypeError 6198: (218-240): Low-level calls are not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 8922: (218-258): Wrong argument count for function call: 2 arguments given but expected 1. This function requires a single bytes argument. If all your arguments are value types, you can use abi.encode(...) to properly generate it.
