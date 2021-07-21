pragma abicoder               v2;

contract C {
    struct S {
        uint a;
        bool x;
    }

    function s() public returns(S memory)
    {
        return S({x: true, a: 8});
    }
}

// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// s() -> refargs { 8, true }
