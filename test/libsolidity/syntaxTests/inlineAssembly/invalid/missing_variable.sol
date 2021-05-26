contract C {
  function f() public pure {
    assembly {
      x := 1
    }
  }
}
// ----
// SyntaxError 1184: (46-75): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
