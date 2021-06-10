contract C
{
    function f(uint x) public {
        assembly {
            x := callvalue()
        }
    }
}
// ----
// SyntaxError 1184: (53-102): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
