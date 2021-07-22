contract C {
    function f(bytes calldata data)
        external
        pure
        returns (uint256[2][3] memory)
    {
        return abi.decode(data, (uint256[2][3]));
    }
}

// ====
// compileViaYul: also
// ----
// f(bytes): "\x06\x05\x04\x03\x02\x01" -> refargs { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 }

