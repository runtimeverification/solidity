pragma abicoder               v2;


contract C {
    struct S {
        uint a;
        uint b;
    }

    function f(uint a, S calldata s, uint b)
        external
        pure
        returns (uint, uint, uint, uint)
    {
        return (a, s.a, s.b, b);
    }
}

// ====
// compileViaYul: also
// ----
// f(uint,(uint,uint),uint): 1, refargs { 2, 3 }, 4 -> 1, 2, 3, 4
