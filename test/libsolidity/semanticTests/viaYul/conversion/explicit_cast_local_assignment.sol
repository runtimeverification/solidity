contract C {
    function f(uint256 a) public pure returns (uint8 x) {
        uint8 b = uint8(a);
        x = b;
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f(uint256): 0x12345678 -> 0x78
