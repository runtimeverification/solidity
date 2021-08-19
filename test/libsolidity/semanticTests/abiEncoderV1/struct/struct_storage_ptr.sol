library L {
	struct S { uint256 x; uint256 y; }
	function f(uint256[] storage r, S storage s) public returns (uint256, uint256, uint256, uint256) {
		r[2] = 8;
		s.x = 7;
		return (r[0], r[1], s.x, s.y);
	}
}
contract C {
	uint8 x = 3;
	L.S s;
	uint256[] r;
	function f() public returns (uint256, uint256, uint256, uint256, uint256, uint256) {
		r = new uint256[](6);
		r[0] = 1;
		r[1] = 2;
		r[2] = 3;
		s.x = 11;
		s.y = 12;
		(uint256 a, uint256 b, uint256 c, uint256 d) = L.f(r, s);
		return (r[2], s.x, a, b, c, d);
	}
}
// ----
// library: L
// f() -> 8, 7, 1, 2, 7, 12
