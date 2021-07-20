pragma abicoder               v2;

struct S { uint a; uint b; }
contract C {
    function f(S calldata s) external pure returns (uint, uint) {
        return (s.a, s.b);
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f((uint,uint)): refargs { 42, 23 } -> 42, 23
