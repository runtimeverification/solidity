contract C {
    uint immutable x;
    function f() public view {
        uint t;
        assembly {
            t := x
        }
    }
}
// ----
// SyntaxError 1184: (90-129): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
