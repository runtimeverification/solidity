contract test {
	constructor() {
		address(this).call("");
	}
}
// ----
// TypeError 6198: (35-53): Low-level calls are not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
