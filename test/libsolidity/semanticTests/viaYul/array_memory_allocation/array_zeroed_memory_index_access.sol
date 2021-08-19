contract C {
	uint test1;
	uint test2;
	uint test3;
	uint test4;
	uint test5;
	uint test6;
	uint test7;
	mapping (string => uint) map;
	function set(string memory s, uint n, uint a) public returns (uint) {
		map[s] = 0;
		uint[] memory x = new uint[](n);
		return x[a];
	}
}
// ====
// compileViaYul: also
// ----
// set(string,uint,uint):"01234567890123456789012345678901", 5, 0 -> 0
// set(string,uint,uint):"01234567890123456789012345678901", 5, 1 -> 0
// set(string,uint,uint):"01234567890123456789012345678901", 5, 4 -> 0
// set(string,uint,uint):"01234567890123456789012345678901", 5, 5 -> FAILURE, 255
