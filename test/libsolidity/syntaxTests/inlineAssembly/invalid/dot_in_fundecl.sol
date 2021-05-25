contract C {
  function f() public pure {
    assembly {
      function f.() {}
      function g.f() {}
    }
  }
}
// ----
// SyntaxError 1184: (46-109): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
