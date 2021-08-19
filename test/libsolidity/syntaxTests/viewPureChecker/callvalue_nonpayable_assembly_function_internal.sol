contract C
{
    function f() internal returns (uint x) {
        assembly {
            x := callvalue()
        }
    }
	function g() public returns (uint) {
		return f();
	}
}
// ----
// SyntaxError 1184: (66-115): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
