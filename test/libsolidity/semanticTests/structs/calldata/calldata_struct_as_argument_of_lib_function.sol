pragma abicoder v2;

struct S {
    uint128 p1;
    uint256[][2] a;
    uint32 p2;
}
struct S1 {
    uint128 u;
    S s;
}

library L {
    function f(S1 memory m, uint32 p) external returns(uint32, uint128, uint256, uint256, uint32) {
        return (p, m.s.p1, m.s.a[0][0], m.s.a[1][1], m.s.p2);
    }
}

contract C {

    function f(S1 calldata c, uint32 p) external returns(uint32, uint128, uint256, uint256, uint32) {
        return L.f(c, p);
    }
}
// ====
// compileViaYul: also
// ----
// library: L
// f((uint128, (uint128, uint256[][2], uint32)), uint32): refargs { 0x0000000000000000000000000000000b, refargs { 0x00000000000000000000000000000016, 0x01, 0x02, 0x0000000000000000000000000000000000000000000000000000000000000001, 0x0000000000000000000000000000000000000000000000000000000000000002, 0x01, 0x02, 0x0000000000000000000000000000000000000000000000000000000000000001, 0x0000000000000000000000000000000000000000000000000000000000000002, 0x00000021 } }, 44 -> 44, 22, 1, 2, 33
