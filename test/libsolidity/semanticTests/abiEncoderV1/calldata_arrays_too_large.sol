contract C {
    function f(uint a, uint[] calldata b, uint c) external pure returns (uint) {
        return 7;
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f(uint,uint[],uint): 6, 0x60, 9, 0x8000000000000000000000000000000000000000000000000000000000000002, 1, 2 -> FAILURE, 255

// This test should pass but takes too much time decoding. Do we want to add a restriction to the array size?
// f(uint,uint[],uint): 6, hex"0102800000000000000000000000000000000000000000000000000000000000000220",  9 -> 7
