contract C {
  function f() public pure {
    assembly {
      function f() {
        leave
      }
    }
  }
}
// ----
// SyntaxError 1184: (46-105): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
