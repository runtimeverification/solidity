contract C {
    function f(uint x) public returns (uint) {
        return this.eval(this.g, x);
    }

    function f2(uint x) public returns (uint) {
        return eval(this.g, x);
    }

    function eval(function(uint) external returns (uint) x, uint a) public returns (uint) {
        return x(a);
    }

    function g(uint x) public returns (uint) {
        return x + 1;
    }
}

// ====
// compileViaYul: also
// ----
// f(uint): 7 -> 8
// f2(uint): 7 -> 8
