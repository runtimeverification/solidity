contract C {
    uint[] x;
    fallback() external {
        assembly {
            x.slot := 1
            x.offset := 2
        }
    }
}
// ----
// SyntaxError 1184: (61-131): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
