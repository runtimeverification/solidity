pragma abicoder               v2;

contract C {
    struct S {
        uint a;
        uint b;
    }
    struct T {
        uint a;
        uint b;
        string s;
    }
    function s() public returns (S memory) {
        return S(23, 42);
    }
    function t() public returns (T memory) {
        return T(23, 42, "any");
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// s() -> refargs { 23, 42 }
// t() -> refargs { 23, 42, "any" }
