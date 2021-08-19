pragma abicoder               v2;
contract C {
    struct S { function (uint) external returns (uint) fn; }
    function f(S calldata s) external returns (uint a) {
        return s.fn(42);
    }
}
// ----
