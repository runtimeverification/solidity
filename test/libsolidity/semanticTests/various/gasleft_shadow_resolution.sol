contract C {
    function gasleft() public returns (uint) {
        return 0;
    }

    function f() public returns (uint) {
        return gasleft();
    }
}

// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f() -> 0
