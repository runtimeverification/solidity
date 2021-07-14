pragma abicoder               v2;
contract C {
    function f(uint256[] calldata x, uint256 i) external returns (uint256) {
        return x[i];
    }
    function f(uint[][] calldata x, uint i, uint j) external returns (uint) {
        return x[i][j];
    }
}
// ====
// compileViaYul: also
// ----
// f(uint256[],uint256): dynarray 256 [ ], 0 -> FAILURE, 255
// f(uint256[],uint256): dynarray 256 [ 23 ], 0 -> 23
// f(uint256[],uint256): dynarray 256 [ 23 ], 1 -> FAILURE, 255
// f(uint256[],uint256): dynarray 256 [ 23 , 42], 0 -> 23
// f(uint256[],uint256): dynarray 256 [ 23 , 42], 1 -> 42
// f(uint256[],uint256): dynarray 256 [ 23 , 42], 2 -> FAILURE, 255
// f(uint[][],uint,uint): refargs { 0x01, 0x00 }, 0, 0 -> FAILURE, 255
// f(uint[][],uint,uint): refargs { 0x01, 0x01, 0x01, 0x01, 23 }, 0, 0 -> 23
