contract C {
    function f(uint256 a, uint256 b) public pure returns (uint256 x) {
        x = a + b;
    }
    function g(uint8 a, uint8 b) public pure returns (uint8 x) {
        x = a + b;
    }
}
// ====
// compileViaYul: also
// ----
// f(uint256,uint256): 5, 6 -> 11
// f(uint256,uint256): 0x00fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffe, 1 -> 0x00ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff
// f(uint256,uint256): 0x00fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffe, 2 -> FAILURE, 255
// f(uint256,uint256): 2, 0x00fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffe -> FAILURE, 255
// g(uint8,uint8): 128, 64 -> 192
// g(uint8,uint8): 128, 127 -> 255
// g(uint8,uint8): 128, 128 -> FAILURE, 255
