contract C {
    function f() public returns (uint256 r) {
        uint;
        uint;
        uint;
        uint;
        int x = -7;
        return uint256(x);
    }
}

// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f() -> -7
