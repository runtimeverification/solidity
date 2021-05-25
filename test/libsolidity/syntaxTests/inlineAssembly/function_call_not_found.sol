contract C {
  function f() public pure {
    assembly {
      k()
    }
  }
}
// ----
// SyntaxError 1184: (46-72): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
