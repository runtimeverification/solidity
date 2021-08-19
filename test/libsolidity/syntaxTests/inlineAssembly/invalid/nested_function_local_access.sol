contract C {
  function f() public pure returns (uint x) {
    assembly {
      function f1() {
        function f2() { }
        x := 2
      }
    }
  }
}
// ----
// SyntaxError 1184: (63-150): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
