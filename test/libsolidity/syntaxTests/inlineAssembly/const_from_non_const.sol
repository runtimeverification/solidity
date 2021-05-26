contract C {
  bool nc = false;
  bool constant c = nc;
  function f() public {
    assembly {
        let t := c
    }
  }
}
// ----
// SyntaxError 1184: (84-119): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 8349: (52-54): Initial value for constant variable has to be compile-time constant.
