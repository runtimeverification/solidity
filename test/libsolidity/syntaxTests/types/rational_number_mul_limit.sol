contract c {
    function f() public pure {
        int256 a;
        a = (1<<4095)*(1<<4095);
    }
}
// ----
// TypeError: (74-93): Type uint is not implicitly convertible to expected type int256.
