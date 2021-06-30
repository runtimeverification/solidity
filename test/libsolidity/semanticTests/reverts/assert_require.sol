contract C {
    function f() public {
        assert(false);
    }

    function g(bool val) public returns (bool) {
        assert(val == true);
        return true;
    }

    function h(bool val) public returns (bool) {
        require(val);
        return true;
    }
}

// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f() -> FAILURE, 255
// g(bool): false -> FAILURE, 255
// g(bool): true -> true
// h(bool): false -> FAILURE, 255
// h(bool): true -> true
