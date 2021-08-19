contract C {
  function f() public pure {
    assembly {
      function linkersymbol(a) {}

      linkersymbol("contract/library.sol:L")
    }
  }
}
// ----
// SyntaxError 1184: (46-142): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
