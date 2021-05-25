contract C {
  function f() public pure {
    assembly {
      let linkersymbol := 1
      let datacopy := 1
      let swap16 := 1
    }
  }
}
// ----
// SyntaxError 1184: (46-136): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
