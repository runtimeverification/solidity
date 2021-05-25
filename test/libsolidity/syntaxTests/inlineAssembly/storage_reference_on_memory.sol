contract C {
    uint[] x;
    fallback() external {
        uint[] memory y = x;
        assembly {
            pop(y.slot)
            pop(y.offset)
        }
    }
}
// ----
// SyntaxError 1184: (90-160): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
