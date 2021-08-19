contract C {
	uint test1;
	uint test2;
	uint test3;
	uint test4;
	uint test5;
	uint test6;
	uint test7;
	mapping (string => uint) map;
	function set(string memory s, uint n, uint m, uint a, uint b) public returns (uint) {
		map[s] = 0;
		uint[][] memory x = new uint[][](n);
		for (uint i = 0; i < n; ++i)
			x[i] = new uint[](m);
		return x[a][b];
	}
}
// ====
// compileViaYul: also
// ----
// set(string,uint,uint,uint,uint): "01234567890123456789012345678901", 2, 4, 0, 0 -> 0
// set(string,uint,uint,uint,uint): "01234567890123456789012345678901", 2, 4, 1, 3 -> 0
// set(string,uint,uint,uint,uint): "01234567890123456789012345678901", 2, 4, 3, 3 -> FAILURE, 255
// set(string,uint,uint,uint,uint): "01234567890123456789012345678901", 2, 4, 1, 5 -> FAILURE, 255
