contract C {
    function f(uint256 a, uint256 b) public pure returns (uint256 x) {
        x = a - b;
    }
    function g(uint8 a, uint8 b) public pure returns (uint8 x) {
        x = a - b;
    }
}
// ====
// compileToEwasm: also
// compileViaYul: also
// ----
// f(uint256,uint256): 6, 5 -> 1
// f(uint256,uint256): 6, 6 -> 0
// f(uint256,uint256): 5, 6 -> FAILURE, 255
// g(uint8,uint8): 6, 5 -> 1
// g(uint8,uint8): 6, 6 -> 0
// g(uint8,uint8): 5, 6 -> FAILURE, 255
