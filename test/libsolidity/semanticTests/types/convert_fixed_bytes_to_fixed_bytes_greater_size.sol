contract Test {
    function bytesToBytes(bytes2 input) public returns (bytes4 ret) {
        return bytes4(input);
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// bytesToBytes(bytes2): 0x6162 -> 0x61620000
