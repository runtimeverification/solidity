contract C {
    function f(int256 a, int256 b) public pure returns (int256) {
        return a % b;
    }
    function g(bool _check) public pure returns (int256) {
        int256 x = type(int256).min;
        if (_check) {
            return x / -1;
        } else {
            unchecked { return x / -1; }
        }
    }
}

// ====
// compileViaYul: also
// ----
// f(int256,int256): 7, 5 -> 2
// f(int256,int256): 7, -5 -> 2
// f(int256,int256): -7, 5 -> -2
// f(int256,int256): -7, 5 -> -2
// f(int256,int256): -5, -5 -> 0
// g(bool): true -> FAILURE, 255
// g(bool): false -> -57896044618658097711785492504343953926634992332820282019728792003956564819968
