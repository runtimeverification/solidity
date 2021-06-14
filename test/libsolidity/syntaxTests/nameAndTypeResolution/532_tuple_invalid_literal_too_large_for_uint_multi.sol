contract C {
    function f() pure public {
        uint x;
        (x, ) = (1E111, 1);
    }
}
// ----
