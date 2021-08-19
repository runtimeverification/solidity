contract test {
    uint constant x = 1;
    function f() public pure {
        assembly {
            let y := x
        }
    }
}
// ----
// SyntaxError 1184: (80-123): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
