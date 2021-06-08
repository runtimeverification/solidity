contract C
{
    function f(uint x) public payable {
        assembly {
            x := callvalue()
        }
    }
}
// ----
// SyntaxError 1184: (61-110): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
