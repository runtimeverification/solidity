contract C {
    uint constant a = 2;
    uint constant b = a;
    function f() public pure {
        assembly {
            let x := b
        }
    }
}
// ----
// SyntaxError 1184: (102-145): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
