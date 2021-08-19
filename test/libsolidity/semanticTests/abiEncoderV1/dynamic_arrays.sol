contract C {
    function f(uint a, uint16[] memory b, uint c)
            public pure returns (uint, uint, uint) {
        return (b.length, b[a], c);
    }
}
// ====
// compileViaYul: also
// ----
// f(uint,uint16[],uint): 6, dynarray 16 [ 11, 12, 13, 14, 15, 16, 17 ], 9 -> 7, 17, 9
