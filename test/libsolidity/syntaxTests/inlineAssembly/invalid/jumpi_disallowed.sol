contract C {
    function f() pure public {
        assembly {
            jumpi(2, 1)
        }
    }
}
// ----
// SyntaxError 1184: (52-96): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
