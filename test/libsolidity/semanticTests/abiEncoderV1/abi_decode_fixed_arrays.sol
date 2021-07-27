contract C {
    function f(uint16[3] memory a, uint16[2][3] memory b, uint i, uint j, uint k)
            public pure returns (uint, uint) {
        return (a[i], b[j][k]);
    }
}
// ====
// compileViaYul: also
// ----
// f(uint16[3],uint16[2][3],uint,uint,uint): array 16 [ 1, 2, 3 ], refargs { 0x000b, 0x000c, 0x0015, 0x0016, 0x001f, 0x0020 }, 1, 2, 1 -> 2, 32
