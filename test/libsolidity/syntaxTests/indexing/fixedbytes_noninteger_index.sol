contract C {
  function f() public {
    bytes32 b;
    b[888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888];
  }
}
// ----
// TypeError 1859: (56-170): Out of bounds array access.
