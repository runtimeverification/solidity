contract C {
  bytes s;
  function f(bytes calldata data) external returns (bytes1) {
    s = data;
    return s[0];
  }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f(bytes): "abcdefgh" -> 0x61
