contract C {
    function f(uint x) public returns (uint) {
        return eval(g, x);
    }

    function eval(function(uint) internal returns (uint) x, uint a) internal returns (uint) {
        return x(a);
    }

    function g(uint x) public pure returns (uint) {
        return x + 1;
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f(uint): 7 -> 8
