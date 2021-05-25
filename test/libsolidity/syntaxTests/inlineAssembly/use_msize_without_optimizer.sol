contract C {
    function f() pure public {
        assembly {
            let x := msize()
        }
    }
}
// ====
// optimize-yul: false
// ----
// SyntaxError 1184: (52-101): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
