contract test {
	constructor(uint x) {
		assembly {
			x := gas()
		}
	}
}
// ----
// SyntaxError 1184: (41-69): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
