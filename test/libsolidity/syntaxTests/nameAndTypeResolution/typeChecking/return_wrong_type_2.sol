contract test {
    function f() public returns (uint r) { return 1 >= 2; }
}
// ----
// TypeError 6359: (66-72): Return argument type bool is not implicitly convertible to expected type (type of first return variable) uint.
