contract C {
    uint constant a;
    function f() public pure {
        assembly {
            let a := 1
        }
    }
}
// ----
// SyntaxError 1184: (73-116): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 4266: (17-32): Uninitialized "constant" variable.
