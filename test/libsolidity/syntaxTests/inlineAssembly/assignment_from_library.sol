library L {
}

contract C {
  function f() public pure {
    assembly {
      let x := L
    }
  }
}
// ----
// SyntaxError 1184: (61-94): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
