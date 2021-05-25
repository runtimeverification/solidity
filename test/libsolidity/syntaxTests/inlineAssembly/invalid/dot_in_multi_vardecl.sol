contract C {
  function f() public pure {
    assembly {
      function f() -> x, y, z {}
      let a., aa.b := f()
    }
  }
}
// ----
// SyntaxError 1184: (46-121): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
