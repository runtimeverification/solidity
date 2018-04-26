contract c {
    function f() public pure {
        int256 a;
        a = 1 << 4095; // shift is fine, but result too large
        a = 1 << 4096; // too large
        a = (1E1233) << 2; // too large
    }
}
// ----
// TypeError: (74-83): Type int_const 5221...(1225 digits omitted)...5168 is not implicitly convertible to expected type int256.
// TypeError: (136-145): Type uint is not implicitly convertible to expected type int256.
// TypeError: (172-185): Type uint is not implicitly convertible to expected type int256.
