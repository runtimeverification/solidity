contract C {
    function f() public pure returns (uint256, bytes memory) {
        bytes memory arg = "abcdefg";
        return abi.decode(abi.encode(uint256(33), arg), (uint256, bytes));
    }
}

// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f() -> 0x21, "abcdefg"
