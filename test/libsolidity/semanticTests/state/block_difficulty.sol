contract C {
    function f() public returns (uint) {
        return block.difficulty;
    }
}
// ====
// compileViaYul: also
// ----
// f() -> 0
// f() -> 0
// f() -> 0
