contract C {
    function f(uint a, bytes memory b, uint c)
            public pure returns (uint, uint, bytes1, uint) {
        return (a, b.length, b[3], c);
    }

    function f_external(uint a, bytes calldata b, uint c)
            external pure returns (uint, uint, bytes1, uint) {
        return (a, b.length, b[3], c);
    }
}
// ====
// compileViaYul: also
// ----
// f(uint,bytes,uint): 6, "abcdefg", 9 -> 6, 7, 0x64, 9
// f_external(uint,bytes,uint): 6, "abcdefg", 9 -> 6, 7, 0x64, 9
