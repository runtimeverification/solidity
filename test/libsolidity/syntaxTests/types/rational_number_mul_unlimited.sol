contract c {
    function f() public pure {
        int a;
        a = (1<<4095)*(1<<4095);
    }
}
// ----
