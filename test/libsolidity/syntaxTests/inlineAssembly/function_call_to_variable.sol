contract C {
  function f() public pure {
    assembly {
      let x := 1

      x()
    }
  }
}
// ----
// SyntaxError 1184: (46-90): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
