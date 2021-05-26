contract C {
    uint[] x;
    fallback() external {
        uint[] storage y = x;
        assembly {
            pop(y)
        }
    }
}
// ----
// SyntaxError 1184: (91-130): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
