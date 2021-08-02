contract test {
	constructor() {
		address(this).delegatecall("");
	}
}
// ----
// TypeError 6198: (35-61): Low-level calls are not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
