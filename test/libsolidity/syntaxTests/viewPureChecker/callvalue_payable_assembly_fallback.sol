contract C
{
    receive () external payable {
    }
    fallback () external payable {
        uint x;
        assembly {
            x := callvalue()
        }
    }
}
// ----
// SyntaxError 1184: (112-161): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
