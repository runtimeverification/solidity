contract test {
	constructor() {
		address(this).callcode("");
	}
}
// ----
// TypeError 6198: (35-57): Low-level calls are not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
