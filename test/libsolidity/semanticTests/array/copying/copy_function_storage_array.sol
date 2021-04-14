contract C {
    function() internal returns (uint)[] x;
    function() internal returns (uint)[] y;

    function test() public returns (uint) {
        x = new function() internal returns (uint)[](10);
        x[9] = a;
        y = x;
        return y[9]();
    }

    function a() public returns (uint) {
        return 7;
    }
}

// ====
// compileViaYul: also
// ----
// test() -> 7
