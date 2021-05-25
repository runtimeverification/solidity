contract C {
  function f() public pure {
    assembly {
      let x := f
    }
  }
}
// ----
// SyntaxError 1184: (46-79): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
