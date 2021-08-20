contract C {
    function f() public returns (bool) {
        return this.f.address == address(this);
    }
    function g(function() external cb) public returns (address) {
        return cb.address;
    }
}
// ====
// compileViaYul: also
// ----
// f() -> true
// g(function): hex"0000000000000000000000000000000000000042", 21 -> 0x42
