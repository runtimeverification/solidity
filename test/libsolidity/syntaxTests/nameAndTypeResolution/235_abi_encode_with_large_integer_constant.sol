contract C {
    function f() pure public { abi.encode(2**500); }
}
// ----
// Warning 6133: (44-62): Statement has no effect.
