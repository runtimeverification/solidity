pragma abicoder               v2;


contract C {
    struct S {
        uint256 a;
        uint256 b;
    }

    function f(S[] calldata s)
        external
        pure
        returns (uint l, uint256 a, uint256 b, uint256 c, uint256 d)
    {
        l = s.length;
        a = s[0].a;
        b = s[0].b;
        c = s[1].a;
        d = s[1].b;
    }
}

// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f((uint256,uint256)[]): refargs { 0x01, 0x02, 0x0000000000000000000000000000000000000000000000000000000000000001, 0x0000000000000000000000000000000000000000000000000000000000000002, 0x0000000000000000000000000000000000000000000000000000000000000003, 0x0000000000000000000000000000000000000000000000000000000000000004 } -> 2, 1, 2, 3, 4
