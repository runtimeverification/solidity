contract C {
    //int256 public a = -0x42 << 8;
    function a() public returns (int256) {
      return -0x42 << 8;
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// a() -> -16896
