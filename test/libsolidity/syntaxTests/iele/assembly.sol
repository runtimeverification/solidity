contract test {
	constructor(uint x) public {
		assembly {
			x := gas
		}
	}
}
// ----
// SyntaxError: (48-77): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
