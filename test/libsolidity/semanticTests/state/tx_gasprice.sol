contract C {
    function f() public returns (uint) {
        return tx.gasprice;
    }
}
// ====
// compileViaYul: also
// ----
// f() -> 0
// f() -> 0
// f() -> 0
