pragma abicoder v2;


contract C {
    struct S {
        uint a;
        uint[2] b;
        uint c;
    }

    function f(S calldata s)
        external
        pure
        returns (uint a, uint b0, uint b1, uint c)
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
// f((uint,uint[2],uint)): refargs { 42, 1, 2, 23 } -> 42, 1, 2, 23
