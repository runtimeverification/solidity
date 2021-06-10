contract test {
    function fun(uint a) public {
        uint x = 3 ** a;
    }
}
// ----
// Warning 2072: (58-64): Unused local variable.
// Warning 2018: (20-80): Function state mutability can be restricted to pure
