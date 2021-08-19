pragma experimental SMTChecker;

contract C
{
	struct S {
		uint256 x;
	}

	mapping (uint256 => S) smap;

	function f(uint256 y, uint256 v) public {
		smap[y] = S(v);
		S memory smem = S(v);
		assert(smap[y].x == smem.x);
	}
}
// ----
