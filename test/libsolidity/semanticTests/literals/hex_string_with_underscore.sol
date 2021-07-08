contract C {
    function f() public pure returns(bytes memory) {
        return hex"12_34_5678_9A";
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f() -> "\x12\x34\x56\x78\x9A"
