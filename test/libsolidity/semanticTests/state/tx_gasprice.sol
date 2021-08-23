contract C {
    function f() public returns (uint) {
        return tx.gasprice;
    }
}
// ====
// compileViaYul: also
// ----
// f() -> 5120
// f() -> 5120
// f() -> 5120
