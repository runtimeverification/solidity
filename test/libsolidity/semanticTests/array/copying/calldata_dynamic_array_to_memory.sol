pragma abicoder               v2;


contract C {
    function f(uint[][] calldata a)
        external
        returns (uint, uint[] memory)
    {
        uint[] memory m = a[0];
        return (a.length, m);
    }
}

// ====
// compileViaYul: also
// ----
// f(uint[][]): refargs { 0x01, 0x01, 0x01, 0x02, 23, 42 } -> 1, refargs { 0x01, 0x02, 23, 42 }
