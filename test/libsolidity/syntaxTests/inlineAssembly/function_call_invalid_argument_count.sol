contract C {
  function f() public pure {
    assembly {
      function f(a) {}

      f()
      f(1)
      f(1, 2)
    }
  }
}
// ----
// SyntaxError 1184: (46-121): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
