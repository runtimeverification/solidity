contract C {
    function f() public returns (uint) {
        return block.gaslimit;
    }
}
// ====
// compileViaYul: also
// ----
// f() -> 8000000
// f() -> 8000000
// f() -> 8000000
