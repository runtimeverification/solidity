contract test {
    uint constant x = 1;
    function f() public {
        assembly {
            x := 2
        }
    }
}
// ----
// SyntaxError 1184: (75-114): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
