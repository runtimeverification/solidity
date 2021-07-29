pragma abicoder               v2;
contract C {
  function f(bytes calldata x) public returns (C[] memory) {
    return abi.decode(x, (C[]));
  }
  function g() public returns (bytes memory) {
    C[] memory c = new C[](3);
    c[0] = C(address(0x42));
    c[1] = C(address(0x21));
    c[2] = C(address(0x23));
    return abi.encode(c);
  }
}
// ====
// compileViaYul: also
// ----
// f(bytes): "\x01\x03\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x03\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" -> dynarray 160 [ 1, 2, 3 ]
// f(bytes): "\x01\x01\x14\x13\x12\x11\x10\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01" -> dynarray 160 [ 0x0102030405060708090a0b0c0d0e0f1011121314 ]
// f(bytes): "\x01\x01\x15\x14\x13\x12\x11\x10\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01" -> dynarray 160 [ 0x02030405060708090a0b0c0d0e0f101112131415 ]
// g() -> "\x01\x03\x42\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x21\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x23\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
