contract C {
    function f() public pure {
        assembly {
            let f := 1
        }
    }
}
// ----
// SyntaxError 1184: (52-95): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
