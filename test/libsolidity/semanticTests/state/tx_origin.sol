contract C {
    function f() public returns (address) {
        return tx.origin;
    }
}
// ====
// compileViaYul: also
// ----
// f() -> sender_address
// f() -> sender_address
// f() -> sender_address
