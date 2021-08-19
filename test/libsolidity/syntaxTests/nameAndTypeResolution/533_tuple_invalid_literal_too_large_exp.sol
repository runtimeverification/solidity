contract C {
    function f() pure public {
        (2**270, 1);
    }
}
// ----
// Warning 6133: (52-63): Statement has no effect.
