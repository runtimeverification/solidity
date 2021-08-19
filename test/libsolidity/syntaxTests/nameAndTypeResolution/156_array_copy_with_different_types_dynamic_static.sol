contract c {
    uint[] a;
    uint[80] b;
    function f() public { b = a; }
}
// ----
// TypeError 7407: (73-74): Type uint[] storage ref is not implicitly convertible to expected type uint[80] storage ref.
