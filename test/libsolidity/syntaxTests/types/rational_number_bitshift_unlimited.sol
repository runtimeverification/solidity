contract c {
    function f() public pure {
        int a;
        a = 1 << 4095; // shift is fine, but result too large
        a = 1 << 4096; // too large
        a = (1E1233) << 2; // too large
    }
}
