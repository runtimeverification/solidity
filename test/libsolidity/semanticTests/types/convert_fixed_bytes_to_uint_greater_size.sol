contract Test {
    function bytesToUint(bytes4 s) public returns (uint64 h) {
        return uint64(uint32(s));
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// bytesToUint(bytes4): 0x61626364 -> 0x61626364
