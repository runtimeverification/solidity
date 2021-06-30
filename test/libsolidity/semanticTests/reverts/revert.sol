contract C {
    uint256 public a = 42;

    function f() public {
        a = 1;
        revert();
    }
}

// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f() -> FAILURE, 255
// a() -> 42
