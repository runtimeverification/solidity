pragma abicoder               v2;

contract C {
	function f(uint a, uint16[][] memory b, uint[2][][3] memory c, uint d)
			public pure returns (uint, uint, uint, uint, uint, uint, uint) {
		return (a, b.length, b[1].length, b[1][1], c[1].length, c[1][1][1], d);
	}
	function test() public view returns (uint, uint, uint, uint, uint, uint, uint) {
		uint16[][] memory b = new uint16[][](3);
		b[0] = new uint16[](2);
		b[0][0] = 0x55;
		b[0][1] = 0x56;
		b[1] = new uint16[](4);
		b[1][0] = 0x65;
		b[1][1] = 0x66;
		b[1][2] = 0x67;
		b[1][3] = 0x68;

		uint[2][][3] memory c;
		c[0] = new uint[2][](1);
		c[0][0][1] = 0x75;
		c[1] = new uint[2][](5);
		c[1][1][1] = 0x85;

		return this.f(12, b, c, 13);
	}
}
// ====
// compileViaYul: also
// ----
// test() -> 12, 3, 4, 0x66, 5, 0x0085, 13
// f(uint,uint16[][],uint[2][][3],uint): 12, refargs { 0x01, 0x03, 0x01, 0x02, 0x0055, 0x0056, 0x01, 0x04, 0x0065, 0x0066, 0x0067, 0x0068, 0x01, 0x00 }, refargs { 0x01, 0x01, 0, 117, 0x01, 0x05, 0, 0, 0, 133, 0, 0, 0, 0, 0, 0, 0x01, 0x00 }, 13 -> 12, 3, 4, 0x66, 5, 0x0085, 13
