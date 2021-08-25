contract C {
    function f(uint a, uint[] calldata b, uint c) external pure returns (uint) {
        return 7;
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f(uint,uint[],uint): 6, hex"010280000000000000000000000000000000000000000000000000000000000000020000000000000020",  9 -> FAILURE, 5
