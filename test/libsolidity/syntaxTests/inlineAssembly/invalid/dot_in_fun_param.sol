contract C {
  function f() public pure {
    assembly {
      function f(a., x.b) -> t.b, b.. {}
    }
  }
}
// ----
// SyntaxError 1184: (46-103): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
