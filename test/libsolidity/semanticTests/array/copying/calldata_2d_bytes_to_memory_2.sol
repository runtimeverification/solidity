pragma abicoder v2;

contract C {
    function g(bytes[2] memory m) internal {
        assert(m[0].length > 1);
        assert(m[1].length > 1);
        assert(m[0][0] == m[1][0]);
        assert(m[0][1] == m[1][1]);
    }
    function f(bytes[2] calldata c) external {
        g(c);
    }
}
// ====
// compileViaYul: also
// ----
// f(bytes[2]): refargs { "ab", "ab" } ->
// f(bytes[2]): refargs { "a", "a" } -> FAILURE, 255
