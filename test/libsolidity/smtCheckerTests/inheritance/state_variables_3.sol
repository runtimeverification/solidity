pragma experimental SMTChecker;

contract Base {
	uint256 x;
	uint256 private t;
}

contract C is Base {

	uint256 private z;
	function f(uint256 y) public {
		require(x < 10);
		require(y < 100);
		z = x + y;
		assert(z < 150);
	}
}
// ----
