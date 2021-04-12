contract c {
    bytes data;

    function test() public returns (bytes memory) {
        for (uint256 i = 0; i < 33; i++) data.push(0x03);
        for (uint256 j = 0; j < 4; j++) data.pop();
        return data;
    }
}

// ====
// compileViaYul: also
// ----
// test() -> "\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03"
