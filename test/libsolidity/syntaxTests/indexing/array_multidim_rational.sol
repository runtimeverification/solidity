contract C {
  function f() public {
    bytes[32] memory a;
    a[8**90][8**90][8**90*0.1];
  }
}
// ----
// TypeError 3383: (65-73): Out of bounds array access.
// TypeError 7407: (81-90): Type rational_const 9485...(73 digits omitted)...5712 / 5 is not implicitly convertible to expected type uint.
// TypeError 6318: (65-91): Index expression cannot be represented as an unsigned integer.
