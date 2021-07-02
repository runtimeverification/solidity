pragma experimental SMTChecker;

contract Base {
	uint256 x;
	uint256 z;
	uint256 private t;
}

contract C is Base {
	function f(uint256 y) public {
		require(x < 10);
		require(y < 100);
		z = x + y;
		assert(z < 150);
	}
}
// ----
