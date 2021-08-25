contract C {
    function f() external returns (address) {
        return this.f.address;
    }
    function g() external returns (bool) {
      return this.f.address == address(this);
    }
    function h(function() external a) public returns (address) {
      return a.address;
    }
}
// ====
// compileViaYul: also
// ----
// f() -> contract_address
// g() -> true
// h(function): 0x1122334400112233445566778899AABBCCDDEEFF, 42 -> 0x1122334400112233445566778899AABBCCDDEEFF
