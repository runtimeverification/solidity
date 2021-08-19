pragma abicoder v2;

contract C {
    struct S {
        uint a;
        uint[2] b;
        uint c;
    }

    function f(S calldata c)
        external
        pure
        returns (uint, uint, uint, uint)
    {
        S memory m = c;
        return (m.a, m.b[0], m.b[1], m.c);
    }
}

// ====
// compileViaYul: also
// ----
// f((uint,uint[2],uint)): refargs { 42, 1, 2, 23 } -> 42, 1, 2, 23
