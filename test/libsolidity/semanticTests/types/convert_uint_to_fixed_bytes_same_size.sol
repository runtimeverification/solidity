contract Test {
    function uintToBytes(uint h) public returns (bytes32 s) {
        return bytes32(h);
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// uintToBytes(uint): left(0x616263) -> left(0x616263)
