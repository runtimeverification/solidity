contract test {
    function fixedBytesHex() public returns(bytes32 ret) {
        return hex"aabb00ff";
    }
    function fixedBytes() public returns(bytes32 ret) {
        return "abc\x00\xff__";
    }
    function pipeThrough(bytes2 small, bool one) public returns(bytes16 large, bool oneRet) {
        oneRet = one;
        large = small;
    }
}

// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// fixedBytesHex() -> 0x00aabb00ff00000000000000000000000000000000000000000000000000000000
// fixedBytes() -> 0x61626300ff5f5f00000000000000000000000000000000000000000000000000
// pipeThrough(bytes2,bool): 0x02, true -> 0x020000000000000000000000000000, true
