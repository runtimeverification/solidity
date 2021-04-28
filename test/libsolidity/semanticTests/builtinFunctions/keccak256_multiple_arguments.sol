contract c {
    function foo(uint256 a, uint256 b, uint256 c) public returns (bytes32 d) {
        d = keccak256(abi.encodePacked(a, b, c));
    }
}
// ====
// compileViaYul: also
// ----
// foo(uint256,uint256,uint256): 0x0a, 0x0c, 0x0d -> 0x00bc740a98aae5923e8f04c9aa798c9ee82f69e319997699f2782c40828db9fd81
