pragma abicoder v2;

contract C {
    struct S {
        uint128 a;
        uint64 b;
        uint128 c;
    }
    function f(S[3] calldata c) public returns (uint128, uint64, uint128) {
        S[3] memory m = c;
        return (m[2].a, m[1].b, m[0].c);
    }
    function g(S[] calldata c) public returns (uint128, uint64, uint128) {
        S[] memory m = c;
        return (m[2].a, m[1].b, m[0].c);
    }
}
// ====
// compileViaYul: also
// ----
// f((uint128, uint64, uint128)[3]): refargs { 0x00000000000000000000000000000000, 0x0000000000000000, 0x0000000000000000000000000000000c, 0x00000000000000000000000000000000, 0x000000000000000b, 0x00000000000000000000000000000000, 0x0000000000000000000000000000000a, 0x0000000000000000, 0x00000000000000000000000000000000 } -> 10, 11, 12
// g((uint128, uint64, uint128)[]): refargs { 0x01, 0x03, 0x00000000000000000000000000000000, 0x0000000000000000, 0x0000000000000000000000000000000c, 0x00000000000000000000000000000000, 0x000000000000000b, 0x00000000000000000000000000000000, 0x0000000000000000000000000000000a, 0x0000000000000000, 0x00000000000000000000000000000000 } -> 10, 11, 12
