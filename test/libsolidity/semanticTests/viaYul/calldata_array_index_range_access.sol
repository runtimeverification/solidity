pragma abicoder               v2;
contract C {
    function f(uint256[] calldata x, uint256 s, uint256 e) external returns (uint) {
        return uint256[](x[s:e]).length;
    }
    function f(uint256[] calldata x, uint256 s, uint256 e, uint256 ss, uint256 ee) external returns (uint) {
        return uint256[](x[s:e][ss:ee]).length;
    }
    function f_s_only(uint256[] calldata x, uint256 s) external returns (uint) {
        return uint256[](x[s:]).length;
    }
    function f_e_only(uint256[] calldata x, uint256 e) external returns (uint) {
        return uint256[](x[:e]).length;
    }
    function g(uint256[] calldata x, uint256 s, uint256 e, uint256 idx) external returns (uint256) {
        return uint256[](x[s:e])[idx];
    }
    function gg(uint256[] calldata x, uint256 s, uint256 e, uint256 idx) external returns (uint256) {
        return x[s:e][idx];
    }
    function gg_s_only(uint256[] calldata x, uint256 s, uint256 idx) external returns (uint256) {
        return x[s:][idx];
    }
    function gg_e_only(uint256[] calldata x, uint256 e, uint256 idx) external returns (uint256) {
        return x[:e][idx];
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f(uint256[],uint256,uint256): dynarray 256 [ 1, 2, 3, 4, 5 ], 2, 4 -> 2
// f(uint256[],uint256,uint256): dynarray 256 [ 1, 2, 3, 4, 5 ], 2, 6 -> FAILURE, 255
// f(uint256[],uint256,uint256): dynarray 256 [ 1, 2, 3, 4, 5 ], 3, 3 -> 0
// f(uint256[],uint256,uint256): dynarray 256 [ 1, 2, 3, 4, 5 ], 4, 3 -> FAILURE, 255
// f(uint256[],uint256,uint256): dynarray 256 [ 1, 2, 3, 4, 5 ], 0, 3 -> 3
// f(uint256[],uint256,uint256,uint256,uint256): dynarray 256 [ 1, 2, 3, 4, 5 ], 1, 3, 1, 2 -> 1
// f(uint256[],uint256,uint256,uint256,uint256): dynarray 256 [ 1, 2, 3, 4, 5 ], 1, 3, 1, 4 -> FAILURE, 255
// f_s_only(uint256[],uint256): dynarray 256 [ 1, 2, 3, 4, 5 ], 2 -> 3
// f_s_only(uint256[],uint256): dynarray 256 [ 1, 2, 3, 4, 5 ], 6 -> FAILURE, 255
// f_e_only(uint256[],uint256): dynarray 256 [ 1, 2, 3, 4, 5 ], 3 -> 3
// f_e_only(uint256[],uint256): dynarray 256 [ 1, 2, 3, 4, 5 ], 6 -> FAILURE, 255
// g(uint256[],uint256,uint256,uint256): dynarray 256 [ 1, 2, 3, 4, 5 ], 2, 4, 1 -> 4
// g(uint256[],uint256,uint256,uint256): dynarray 256 [ 1, 2, 3, 4, 5 ], 2, 4, 3 -> FAILURE, 255
// gg(uint256[],uint256,uint256,uint256): dynarray 256 [ 1, 2, 3, 4, 5 ], 2, 4, 1 -> 4
// gg(uint256[],uint256,uint256,uint256): dynarray 256 [ 1, 2, 3, 4, 5 ], 2, 4, 3 -> FAILURE, 255
