contract C {
    function f() public pure returns (string memory) {
        return "";
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f() -> ""
