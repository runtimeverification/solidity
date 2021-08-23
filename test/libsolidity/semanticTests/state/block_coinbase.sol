contract C {
    function f() public returns (address payable) {
        return block.coinbase;
    }
}
// ====
// compileViaYul: also
// ----
// f() -> 0x0
// f() -> 0x0
// f() -> 0x0
