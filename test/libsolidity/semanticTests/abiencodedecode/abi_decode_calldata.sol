contract C {
    function f(bytes calldata data)
        external
        pure
        returns (uint256, bytes memory r)
    {
        return abi.decode(data, (uint256, bytes));
    }
}

// ====
// compileViaYul: also
// ----
// f(bytes): "\x21\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x07\x00\x00\x00\x00\x00\x00\x00gfedcba" -> 0x21, "abcdefg"
