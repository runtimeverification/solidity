contract C {
    function f() public pure {
        assembly {
            let x.offset := 1
            let x.slot := 1
        }
    }
}
// ----
// SyntaxError 1184: (52-130): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
