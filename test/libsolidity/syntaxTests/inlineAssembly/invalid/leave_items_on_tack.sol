contract C {
    function f() pure public {
        assembly {
            mload(0)
        }
    }
}
// ----
// SyntaxError 1184: (52-93): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
