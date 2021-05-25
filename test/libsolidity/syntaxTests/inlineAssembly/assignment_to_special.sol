contract C {
  function f() public {
    assembly {
      super := 1
      this := 1
      msg := 1
      block := 1
      f := 1
      C := 1
    }
  }
}
// ----
// SyntaxError 1184: (41-148): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
