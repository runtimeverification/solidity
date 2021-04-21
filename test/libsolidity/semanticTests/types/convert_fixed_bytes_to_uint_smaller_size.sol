contract Test {
    function bytesToUint(bytes4 s) public returns (uint16 h) {
        return uint16(uint32(s));
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// bytesToUint(bytes4): 0x61626364 -> 0x6364
