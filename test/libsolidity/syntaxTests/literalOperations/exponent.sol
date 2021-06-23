contract C {
    function g() public pure {
        int256 a;
        a ** 1E1233;
        a ** (1/2);
    }
}
// ----
// TypeError 2271: (70-81): Operator ** not compatible with types int256 and int_const 1000...(1226 digits omitted)...0000. Exponent too large.
// TypeError 2271: (91-101): Operator ** not compatible with types int256 and rational_const 1 / 2. Exponent is fractional.
