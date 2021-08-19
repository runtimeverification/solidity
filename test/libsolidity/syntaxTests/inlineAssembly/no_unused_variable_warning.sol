contract C {
    function f() pure public {
        uint a;
        assembly {
            a := 1
        }
    }
}
// ----
// SyntaxError 1184: (68-107): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
