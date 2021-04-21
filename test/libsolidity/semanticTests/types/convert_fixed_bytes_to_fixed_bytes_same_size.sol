contract Test {
    function bytesToBytes(bytes4 input) public returns (bytes4 ret) {
        return bytes4(input);
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// bytesToBytes(bytes4): 0x61626364 -> 0x61626364
