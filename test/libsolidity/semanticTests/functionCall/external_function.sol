contract c {
    function f(uint a) public returns (uint) {
        return a;
    }

    function test(uint a, uint b)
        external
        returns (uint r_a, uint r_b)
    {
        r_a = f(a + 7);
        r_b = b;
    }
}

// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// test(uint,uint): 2, 3 -> 9, 3
