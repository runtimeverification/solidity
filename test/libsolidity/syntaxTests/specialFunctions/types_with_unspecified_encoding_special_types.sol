contract C {
    function f() public pure {
        (bool a,) = address(this).call(abi.encode(address(this).delegatecall, super));
        (a,) = address(this).delegatecall(abi.encode(block, tx, mulmod));
        a;
    }
}
// ----
// TypeError: (61-114): Low-level calls are not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 2056: (94-120): This type cannot be encoded.
// TypeError 2056: (122-127): This type cannot be encoded.
// TypeError: (133-177): Low-level calls are not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 2056: (184-189): This type cannot be encoded.
// TypeError 2056: (191-193): This type cannot be encoded.
// TypeError 2056: (195-201): This type cannot be encoded.
