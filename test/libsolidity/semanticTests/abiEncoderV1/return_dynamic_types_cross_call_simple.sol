contract C {
    function dyn() public returns (bytes memory) {
        return "1234567890123456789012345678901234567890";
    }
    function f() public returns (bytes memory) {
        return this.dyn();
    }
}
// ====
// compileViaYul: also
// EVMVersion: >homestead
// ----
// f() -> "1234567890123456789012345678901234567890"
