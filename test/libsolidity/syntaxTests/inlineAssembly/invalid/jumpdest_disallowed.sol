contract C {
    function f() pure public {
        assembly {
            jumpdest()
        }
    }
}
// ----
// SyntaxError 1184: (52-95): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
