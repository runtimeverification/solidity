contract C {
	function f() pure public {
		assembly {
			pop(pc())
		}
	}
}
// ----
// SyntaxError 1184: (43-70): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
