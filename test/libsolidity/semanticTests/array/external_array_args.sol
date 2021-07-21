contract c {
    function test(uint[8] calldata a, uint[] calldata b, uint[5] calldata c, uint a_index, uint b_index, uint c_index)
            external returns (uint av, uint bv, uint cv) {
        av = a[a_index];
        bv = b[b_index];
        cv = c[c_index];
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// test(uint[8],uint[],uint[5],uint,uint,uint): array 0 [ 1, 2, 3, 4, 5, 6, 7, 8 ], dynarray 0 [ 11, 12, 13 ], array 0 [ 21, 22, 23, 24, 25 ], 0, 1, 2 -> 1, 12, 23
