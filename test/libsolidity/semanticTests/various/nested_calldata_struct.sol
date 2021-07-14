pragma abicoder               v2;


contract C {
    struct S1 {
        uint a;
        uint b;
    }
    struct S2 {
        uint a;
        uint b;
        S1 s;
        uint c;
    }

    function f(S2 calldata s)
        external
        pure
        returns (uint a, uint b, uint sa, uint sb, uint c)
    {
        return (s.a, s.b, s.s.a, s.s.b, s.c);
    }
}

// ====
// compileViaYul: also
// ----
// f((uint,uint,(uint,uint),uint)): refargs { 1, 2, 3, 4, 5 } -> 1, 2, 3, 4, 5
