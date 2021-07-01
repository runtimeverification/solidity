contract C {
    function f() internal pure {
        uint i = 31415999999999999999999999999999999999999999999999999999999999999999933**3;
        uint unreachable = 123;
    }
}
// ----
// Warning 2072: (54-60): Unused local variable.
// Warning 2072: (147-163): Unused local variable.
