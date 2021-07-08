pragma abicoder               v2;
contract C {
	function f(uint a, uint[] calldata b, uint c) external pure returns (uint) {
		return 7;
	}
}
// ====
// EVMVersion: >=byzantium
// revertStrings: debug
// compileViaYul: also
// ----
// f(uint,uint[],uint): 6, refargs {0x01, 0x03, 9, 0x1000000000000000000000000000000000000000000000000000000000000002, 1 } , 2 -> 7
