contract C {
    function f(uint a) public pure {
        assembly {
            let a := 1
        }
    }
}
// ----
// SyntaxError 1184: (58-101): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
