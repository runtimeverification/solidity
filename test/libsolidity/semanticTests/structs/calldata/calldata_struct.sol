pragma abicoder               v2;


contract C {
    struct S {
        uint a;
        uint b;
    }

    function f(S calldata s) external pure returns (uint a, uint b) {
        a = s.a;
        b = s.b;
    }
}

// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f((uint,uint)): refargs { 42, 23 } -> 42, 23
