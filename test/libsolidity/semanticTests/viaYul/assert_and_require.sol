contract C {
    function f(bool a) public pure returns (bool x) {
        bool b = a;
        x = b;
        assert(b);
    }
    function f2(bool a) public pure returns (bool x) {
        bool b = a;
        x = b;
        require(b);
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f(bool): true -> true
// f(bool): false -> FAILURE, 255
// f2(bool): true -> true
// f2(bool): false -> FAILURE, 255
