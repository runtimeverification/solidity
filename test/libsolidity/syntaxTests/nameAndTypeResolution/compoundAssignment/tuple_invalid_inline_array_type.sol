contract C {
    function f() pure public {
        uint x;
        (x, ) = ([100e100]);
    }
}
// ----
// TypeError 7407: (76-87): Type uint[1] memory is not implicitly convertible to expected type tuple(uint,).
