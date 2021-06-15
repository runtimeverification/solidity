contract C {
    function f() external view {}
	function test(address a) external view returns (bool status) {
		// This used to incorrectly raise an error about violating the view mutability.
		(status,) = a.staticcall{gas: 42}("");
		this.f{gas: 42}();
	}
}
// ====
// EVMVersion: >=byzantium
// ----
// TypeError 6198: (207-219): Low-level calls are not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
