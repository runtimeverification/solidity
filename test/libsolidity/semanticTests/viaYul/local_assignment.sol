contract C {
    function f(uint256 a) public pure returns (uint256 x) {
        uint256 b = a;
        x = b;
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f(uint256): 6 -> 6
