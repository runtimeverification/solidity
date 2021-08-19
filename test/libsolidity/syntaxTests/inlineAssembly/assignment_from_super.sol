contract C {
  function f() public pure {
    assembly {
      let x := super
    }
  }
}
// ----
// SyntaxError 1184: (46-83): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
