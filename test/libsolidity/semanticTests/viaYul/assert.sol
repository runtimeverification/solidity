contract C {
    function f(bool a) public pure returns (bool x) {
        bool b = a;
        x = b;
        assert(b);
    }
    function fail() public pure returns (bool x) {
        x = true;
        assert(false);
    }
    function succeed() public pure returns (bool x) {
        x = true;
        assert(true);
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f(bool): true -> true
// f(bool): false -> FAILURE, 255
// fail() -> FAILURE, 255
// succeed() -> true
