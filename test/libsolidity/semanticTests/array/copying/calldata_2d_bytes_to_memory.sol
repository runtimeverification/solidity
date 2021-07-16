pragma abicoder v2;

contract C {
    function g(bytes[2] memory m) internal returns (bytes memory) {
        return m[0];
    }
    function f(bytes[2] calldata c) external returns (bytes memory) {
        return g(c);
    }
}
// ====
// compileViaYul: also
// ----
// f(bytes[2]): refargs { "ab", "ab" } -> "ab"
