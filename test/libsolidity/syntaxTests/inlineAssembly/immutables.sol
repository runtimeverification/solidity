contract C {
  function f() public pure {
    assembly {
      setimmutable("abc", 0)
      loadimmutable("abc")
    }
  }
}
// ----
// SyntaxError 1184: (46-118): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
