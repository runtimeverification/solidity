pragma experimental "v0.5.0";
contract test {
    function f() pure public {
        { uint x; }
        uint x;
    }
}
// ----
// Warning: (87-93): Unused local variable.
// Warning: (105-111): Unused local variable.
