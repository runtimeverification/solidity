contract C {
  bool constant c = this;
  function f() public {
    assembly {
        let t := c
    }
  }
}
// ----
//  SyntaxError 1184: (67-102): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
//  TypeError 7407: (33-37): Type contract C is not implicitly convertible to expected type bool.
//  TypeError 8349: (33-37): Initial value for constant variable has to be compile-time constant.
