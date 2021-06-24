contract c {
    function f() public pure {
        int256 a;
        a = (1<<4095)*(1<<4095);
    }
}
// ----
// TypeError 7407: (74-93): Type int_const 2726...(2458 digits omitted)...8224 is not implicitly convertible to expected type int256. Literal is too large to fit in int256.
