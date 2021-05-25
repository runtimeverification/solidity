contract C {
    function f() pure public {
        uint x; uint y;
        assembly { x, y := 7 }
    }
}
// ----
// SyntaxError 1184: (76-98): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
