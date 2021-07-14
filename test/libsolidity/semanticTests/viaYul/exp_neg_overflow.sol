contract C {
    function f(int8 x, uint256 y) public returns (int256) {
        return x**y;
    }
    function g(int256 x, uint256 y) public returns (int256) {
        return x**y;
    }
}
// ====
// compileViaYul: also
// ----
// f(int8,uint256): 2, 6 -> 64
// f(int8,uint256): 2, 7 -> FAILURE, 255
// f(int8,uint256): 2, 8 -> FAILURE, 255
// f(int8,uint256): -2, 6 -> 64
// f(int8,uint256): -2, 7 -> -128
// f(int8,uint256): -2, 8 -> FAILURE, 255
// f(int8,uint256): 6, 3 -> FAILURE, 255
// f(int8,uint256): 7, 2 -> 0x31
// f(int8,uint256): 7, 3 -> FAILURE, 255
// f(int8,uint256): -7, 2 -> 0x31
// f(int8,uint256): -7, 3 -> FAILURE, 255
// f(int8,uint256): -7, 4 -> FAILURE, 255
// f(int8,uint256): 127, 31 -> FAILURE, 255
// f(int8,uint256): 127, 131 -> FAILURE, 255
// f(int8,uint256): -128, 0 -> 1
// f(int8,uint256): -128, 1 -> -128
// f(int8,uint256): -128, 31 -> FAILURE, 255
// f(int8,uint256): -128, 131 -> FAILURE, 255
// f(int8,uint256): -11, 2 -> 121
// f(int8,uint256): -12, 2 -> FAILURE, 255
// f(int8,uint256): 12, 2 -> FAILURE, 255
// f(int8,uint256): -5, 3 -> -125
// f(int8,uint256): -6, 3 -> FAILURE, 255
// g(int256,uint256): -7, 90 -> 11450477594321044359340126713545146077054004823284978858214566372120240027249
// g(int256,uint256): -7, 91 -> FAILURE, 255
// g(int256,uint256): -63, 42 -> 3735107253208426854890677539053540390278853997836851167913009474475553834369
// g(int256,uint256): -63, 43 -> FAILURE, 255
