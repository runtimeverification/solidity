contract C {
    function f() pure public {
        assembly {
            jump(2)
        }
    }
}
// ----
// SyntaxError 1184: (52-92): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
