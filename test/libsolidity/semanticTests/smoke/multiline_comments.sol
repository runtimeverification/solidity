contract C {
    function f(uint256 a, uint256 b, uint256 c, uint256 d, uint256 e) public returns (uint256) {
        return a + b + c + d + e;
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f(uint256,uint256,uint256,uint256,uint256): 1, 1, 1, 1, 1
// # A comment on the function parameters. #
// -> 5
// f(uint256,uint256,uint256,uint256,uint256):
// 1,
// 1,
// 1,
// 1,
// 1
// -> 5
// # Should return sum of all parameters. #
