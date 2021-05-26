function ff() {}

contract C {
  function f() public pure {
    assembly {
      let x := ff
    }
  }
}
// ----
// SyntaxError 1184: (64-98): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
