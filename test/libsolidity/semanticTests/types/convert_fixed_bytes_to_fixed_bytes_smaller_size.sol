contract Test {
    function bytesToBytes(bytes4 input) public returns (bytes2 ret) {
        return bytes2(input);
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// bytesToBytes(bytes4): 0x61626364 -> 0x6162
