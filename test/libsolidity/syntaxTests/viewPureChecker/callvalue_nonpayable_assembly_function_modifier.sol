contract C
{
	modifier m {
		uint x;
		assembly {
			x := callvalue()
		}
		_;
	}
    function f() m public {
    }
}
// ----
// SyntaxError 1184: (39-73): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
