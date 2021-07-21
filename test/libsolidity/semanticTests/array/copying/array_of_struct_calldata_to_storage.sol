pragma abicoder               v2;

contract C {
    struct S {
        uint128 a;
        uint64 b;
        uint128 c;
    }
    uint128[137] unused;
    S[] s;
    function f(S[] calldata c) public returns (uint128, uint64, uint128) {
        s = c;
        return (s[2].a, s[1].b, s[0].c);
    }
}
// ====
// compileViaYul: true
// ----
// f((uint128, uint64, uint128)[]): refargs { 0x01, 0x03, 0x00000000000000000000000000000000, 0x0000000000000000, 0x0000000000000000000000000000000c, 0x00000000000000000000000000000000, 0x000000000000000b, 0x00000000000000000000000000000000, 0x0000000000000000000000000000000a, 0x0000000000000000, 0x00000000000000000000000000000000 } -> 10, 11, 12
