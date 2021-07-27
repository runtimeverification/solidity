contract C {
	function dyn() public returns (bytes memory a, uint b, bytes20[] memory c, uint d) {
		a = "1234567890123456789012345678901234567890";
		b = type(uint256).max;
		c = new bytes20[](4);
		c[0] = bytes20(uint160(1234));
		c[3] = bytes20(uint160(6789));
		d = 0x1234;
	}
	function f() public returns (bytes memory, uint, bytes20[] memory, uint) {
		return this.dyn();
	}
}
// ====
// compileViaYul: also
// EVMVersion: >homestead
// ----
// f() -> "1234567890123456789012345678901234567890", 0x00ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff, refargs { 0x01, 0x04, 0x00000000000000000000000000000000000004d2, 0x0000000000000000000000000000000000000000, 0x0000000000000000000000000000000000000000, 0x1a85 } , 0x1234
