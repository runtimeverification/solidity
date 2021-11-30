contract C {
    function f() public returns (uint) {
        return block.gaslimit;
    }
}
// ====
// compileViaYul: also
// ----
// f() -> 1435854161
// f() -> 1437256361
// f() -> 1438659930
