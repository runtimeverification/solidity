contract C {
  function f() {
    assembly {
      c := add(add(1, 2), c)
    }
  }
  int constant c = 0 + 1;
}
// ----
// SyntaxError 4937: (15-83): No visibility specified. Did you intend to add "public"?
// SyntaxError 1184: (34-79): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
