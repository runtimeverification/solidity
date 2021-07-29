contract test {
	constructor() {
		this.delegatecall();
	}
}
// ----
// TypeError 3125: (35-52): Member "delegatecall" not found or not visible after argument-dependent lookup in contract test. Use "address(this).delegatecall" to access this address member.
