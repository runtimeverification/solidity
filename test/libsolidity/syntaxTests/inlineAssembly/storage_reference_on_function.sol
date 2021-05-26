contract C {
    function f() pure public {
        assembly {
            let x := f.slot
        }
    }
}
// ----
// SyntaxError 1184: (52-100): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
