contract test {
	uint value1;
	uint value2;
	function f(uint x, uint y) public returns (uint w) {
		uint value3 = y;
		value1 += x;
		value3 *= x;
		value2 *= value3 + value1;
		return value2 += 7;
	}
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f(uint,uint): 0, 6 -> 7
// f(uint,uint): 1, 3 -> 0x23
// f(uint,uint): 2, 25 -> 0x0746
// f(uint,uint): 3, 69 -> 396613
// f(uint,uint): 4, 84 -> 137228105
// f(uint,uint): 5, 2 -> 0xcc7c5e28
// f(uint,uint): 6, 51 -> 1121839760671
// f(uint,uint): 7, 48 -> 408349672884251
