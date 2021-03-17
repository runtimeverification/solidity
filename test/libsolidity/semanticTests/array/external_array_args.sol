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
// test(uint256[8],uint[],uint[5],uint,uint,uint): 1, 2, 3, 4, 5, 6, 7, 8, 0x220, 21, 22, 23, 24, 25, 0, 1, 2, 3, 11, 12, 13 -> 1, 12, 23
