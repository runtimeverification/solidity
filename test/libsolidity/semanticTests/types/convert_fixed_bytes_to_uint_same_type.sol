contract Test {
    function bytesToUint(bytes32 s) public returns (uint h) {
        return uint(s);
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// bytesToUint(bytes32): "abc2" -> left(0x61626332)
