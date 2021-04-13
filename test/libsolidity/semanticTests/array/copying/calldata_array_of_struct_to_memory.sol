pragma abicoder               v2;


contract C {
    struct S {
        uint a;
        uint b;
    }

    function f(S[] memory s)
        external
        pure
        returns (uint l, uint a, uint b, uint c, uint d)
    {
        S[] memory m = s;
        l = m.length;
        a = m[0].a;
        b = m[0].b;
        c = m[1].a;
        d = m[1].b;
    }
}

// ====
// compileViaYul: also
// ----
// f((uint,uint)[]): refargs { 0x01, 0x02, 1, 2, 3, 4 } -> 2, 1, 2, 3, 4
