contract C {
  function f() public pure {
    assembly {
      pop(add(add(1, 2), c))
    }
  }
  int constant c = 1;
}
// ----
// SyntaxError 1184: (46-91): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
