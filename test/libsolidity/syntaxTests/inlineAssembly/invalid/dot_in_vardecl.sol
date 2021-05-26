contract C {
  function f() public pure {
    assembly {
      let a. := 2
      let a.. := 2
      let a.b := 2
      let a..b := 2
    }
  }
}
// ----
// SyntaxError 1184: (46-138): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
