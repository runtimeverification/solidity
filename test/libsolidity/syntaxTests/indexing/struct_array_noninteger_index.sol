contract test {
    struct s { uint a; uint b;}
    function f() pure public returns (bytes1) {
        s[75555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555];
        s[7];
    }
}

// ----
// Warning 6321: (86-92): Unnamed return variable can remain unassigned. Add an explicit return with value to all non-reverting code paths or name the variable.
// Warning 6133: (104-247): Statement has no effect.
// Warning 6133: (257-261): Statement has no effect.
