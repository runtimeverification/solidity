contract C {
    function f() public returns (bytes memory x) {
        x = "12345";
        x[3] = 0x61;
        x[0] = 0x62;
    }
}

// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f() -> "b23a5"
