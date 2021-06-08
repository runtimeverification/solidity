contract C
{
    fallback() external {
        uint x;
        assembly {
            x := callvalue()
        }
    }
}
// ----
// SyntaxError 1184: (63-112): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
