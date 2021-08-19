contract D {
    uint constant a;
}
contract C {
    function f() public pure {
        assembly {
            let D.a := 1
            let D.b := 1 // shadowing the prefix only is also an error
        }
    }
}
// ----
// SyntaxError 1184: (88-204): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 4266: (17-32): Uninitialized "constant" variable.
