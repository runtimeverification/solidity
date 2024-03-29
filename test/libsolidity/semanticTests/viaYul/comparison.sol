contract C {
    function f(address a) public pure returns (bool) {
        return a == address(0);
    }
    function g() public pure returns (bool) {
        return bytes3("abc") == bytes4("abc");
    }
    function lt(uint256 a, uint256 b) public pure returns (bool) {
        return a < b;
    }
    function slt(int256 a, int256 b) public pure returns (bool) {
        return a < b;
    }
    function lte(uint256 a, uint256 b) public pure returns (bool) {
        return a <= b;
    }
    function slte(int256 a, int256 b) public pure returns (bool) {
        return a <= b;
    }
    function gt(uint256 a, uint256 b) public pure returns (bool) {
        return a > b;
    }
    function sgt(int256 a, int256 b) public pure returns (bool) {
        return a > b;
    }
    function gte(uint256 a, uint256 b) public pure returns (bool) {
        return a >= b;
    }
    function sgte(int256 a, int256 b) public pure returns (bool) {
        return a >= b;
    }
    function eq(uint256 a, uint256 b) public pure returns (bool) {
        return a == b;
    }
    function neq(uint256 a, uint256 b) public pure returns (bool) {
        return a != b;
    }
}
// ====
// compileViaYul: also
// ----
// f(address): 0x1234 -> false
// f(address): 0x00 -> true
// g() -> true
// lt(uint256,uint256): 4, 5 -> true
// lt(uint256,uint256): 5, 5 -> false
// lt(uint256,uint256): 6, 5 -> false
// gt(uint256,uint256): 4, 5 -> false
// gt(uint256,uint256): 5, 5 -> false
// gt(uint256,uint256): 6, 5 -> true
// lte(uint256,uint256): 4, 5 -> true
// lte(uint256,uint256): 5, 5 -> true
// lte(uint256,uint256): 6, 5 -> false
// gte(uint256,uint256): 4, 5 -> false
// gte(uint256,uint256): 5, 5 -> true
// gte(uint256,uint256): 6, 5 -> true
// eq(uint256,uint256): 4, 5 -> false
// eq(uint256,uint256): 5, 5 -> true
// eq(uint256,uint256): 6, 5 -> false
// neq(uint256,uint256): 4, 5 -> true
// neq(uint256,uint256): 5, 5 -> false
// neq(uint256,uint256): 6, 5 -> true
// slt(int256,int256): -1, 0 -> true
// slt(int256,int256): 0, 0 -> false
// slt(int256,int256): 1, 0 -> false
// sgt(int256,int256): -1, 0 -> false
// sgt(int256,int256): 0, 0 -> false
// sgt(int256,int256): 1, 0 -> true
// slte(int256,int256): -1, 0 -> true
// slte(int256,int256): 0, 0 -> true
// slte(int256,int256): 1, 0 -> false
// sgte(int256,int256): -1, 0 -> false
// sgte(int256,int256): 0, 0 -> true
// sgte(int256,int256): 1, 0 -> true
