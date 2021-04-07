contract C {
  function f() public returns (uint256 y) {
    unchecked{{
        uint256 max = type(uint256).max;
        uint256 x = max + 1;
        y = x;
    }}
  }
}
// ====
// compileViaYul: also
// ----
// f() -> 0x00
