pragma abicoder v2;

contract C {
    struct S {
        uint32 a;
        uint256[] b;
        uint64 c;
    }

    function f(S calldata s)
        external
        pure
        returns (uint32 a, uint256 b0, uint256 b1, uint64 c)
    {
        a = s.a;
        b0 = s.b[0];
        b1 = s.b[1];
        c = s.c;
    }
}
// ====
// compileViaYul: also
// ----
// f((uint32,uint256[],uint64)): refargs { 0x0000002a, 0x01, 0x02, 0x0000000000000000000000000000000000000000000000000000000000000001, 0x0000000000000000000000000000000000000000000000000000000000000002, 0x0000000000000017 } -> 42, 1, 2, 23
