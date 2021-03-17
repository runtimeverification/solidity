contract c {
    function foo(uint a, uint b, uint c) public returns (bytes32 d) {
        d = keccak256(abi.encodePacked(a, b, c));
    }
}
// ====
// compileViaYul: also
// ----
// foo(uint,uint,uint): 0xa, 0xc, 0xd -> 0x00bc740a98aae5923e8f04c9aa798c9ee82f69e319997699f2782c40828db9fd81
