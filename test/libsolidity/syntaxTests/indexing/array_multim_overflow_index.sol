contract C {
  function f() public {
    bytes[32] memory a;
    a[8**90][8**90][1 - 8**90];
  }
}
// ----
// TypeError 3383: (65-73): Out of bounds array access.
// TypeError 7407: (81-90): Type int_const -189...(75 digits omitted)...1423 is not implicitly convertible to expected type uint. Cannot implicitly convert signed literal to unsigned type.
// TypeError 6318: (65-91): Index expression cannot be represented as an unsigned integer.
