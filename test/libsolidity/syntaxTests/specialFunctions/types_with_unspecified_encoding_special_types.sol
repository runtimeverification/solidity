contract C {
    function f() public pure {
        bool a = address(this).call(address(this).delegatecall, super);
        bool b = address(this).delegatecall(log0, tx, mulmod);
        a; b;
    }
}
// ----
// TypeError: (61-114): Low-level calls are not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError: (80-106): This type cannot be encoded.
// TypeError: (108-113): This type cannot be encoded.
// TypeError: (133-177): Low-level calls are not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError: (160-164): This type cannot be encoded.
// TypeError: (166-168): This type cannot be encoded.
// TypeError: (170-176): This type cannot be encoded.
