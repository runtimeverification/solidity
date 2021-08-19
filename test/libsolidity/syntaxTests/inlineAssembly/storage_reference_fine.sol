contract C {
    uint[] x;
    fallback() external {
        uint[] storage y = x;
        assembly {
            pop(y.slot)
            pop(y.offset)
        }
    }
}
// ----
// SyntaxError 1184: (91-161): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
