contract C {
  function f(bytes calldata data) external returns (bytes memory) {
    return abi.encode(bytes(data));
  }
}
// ====
// compileViaYul: also
// ----
// f(bytes): "abcdefgh" -> "\x08\x00\x00\x00\x00\x00\x00\x00hgfedcba"
