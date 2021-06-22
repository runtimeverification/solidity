contract C {
    enum Test { One, Two }
    function f() public pure {
        Test a = Test(0);
        Test b = Test(1);
        Test c = Test(type(uint256).max);
        a; b; c;
    }
}
// ----
