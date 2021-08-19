contract C {
    function f(uint256 a, uint256 b) public pure returns (uint256 x, uint256 y) {
        x = a;
        y = b;
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f(uint256,uint256): 5, 6 -> 5, 6
