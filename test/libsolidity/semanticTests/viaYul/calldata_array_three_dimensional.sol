pragma abicoder               v2;

contract C {
	function f(uint[][2][] calldata x, uint i, uint j, uint k) external returns (uint a, uint b, uint c, uint d) {
		a = x.length;
		b = x[i].length;
		c = x[i][j].length;
		d = x[i][j][k];
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f(uint[][2][],uint,uint,uint): refargs { 0x01, 0x01, 0x01, 0x01, 42, 0x01, 0x01, 23 }, 0, 0, 0 -> 1, 2, 1, 42
// f(uint[][2][],uint,uint,uint): refargs { 0x01, 0x01, 0x01, 0x01, 42, 0x01, 0x01, 23 }, 0, 1, 0 -> 1, 2, 1, 23
// f(uint[][2][],uint,uint,uint): refargs { 0x01, 0x01, 0x01, 0x01, 42, 0x01, 0x02, 23, 17 }, 0, 1, 0 -> 1, 2, 2, 23
// f(uint[][2][],uint,uint,uint): refargs { 0x01, 0x01, 0x01, 0x01, 42, 0x01, 0x02, 23, 17 }, 0, 1, 1 -> 1, 2, 2, 17
// f(uint[][2][],uint,uint,uint): refargs { 0x01, 0x01, 0x01, 0x01, 42, 0x01, 0x01, 23 }, 1, 0, 0 -> FAILURE, 255
// f(uint[][2][],uint,uint,uint): refargs { 0x01, 0x01, 0x01, 0x01, 42, 0x01, 0x01, 23 }, 0, 2, 0 -> FAILURE, 255
// f(uint[][2][],uint,uint,uint): refargs { 0x01, 0x01, 0x01, 0x01, 42, 0x01, 0x01, 23 }, 0, 2, 0 -> FAILURE, 255
