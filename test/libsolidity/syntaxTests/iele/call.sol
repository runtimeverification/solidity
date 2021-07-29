contract test {
	constructor() {
		this.call();
	}
}
// ----
// TypeError 3125: (35-44): Member "call" not found or not visible after argument-dependent lookup in contract test. Use "address(this).call" to access this address member.
