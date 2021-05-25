contract C {
  function f() public pure {
    assembly {
      switch codesize()
      case "1" {}
      case "2" {}
    }
  }
}
// ----
// SyntaxError 1184: (46-122): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
