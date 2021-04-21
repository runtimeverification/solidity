contract Test {
    function bytesToUint(bytes32 s) public returns (uint256 h) {
        return uint256(s);
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// bytesToUint(bytes32): 0x61626332 -> left(0x61626332)
