// Tests that rational numbers (even negative ones) are encoded properly.
contract C {
    function f() public pure returns (bytes memory) {
        return abi.encode(1, -2);
    }
}

// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f() -> "\x01\xfe"
