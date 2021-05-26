contract c {
    uint8 x;
    function f() public {
        assembly { pop(x) }
    }
}
// ----
// SyntaxError 1184: (60-79): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
