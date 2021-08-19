pragma experimental SMTChecker;

contract Base1 {
	uint256 x;
}

contract Base2 is Base1 {
	uint256 z;
}

contract C is Base2 {
	function f(uint256 y) public {
		require(x < 10);
		require(y < 100);
		z = x + y;
		assert(z < 150);
	}
}
// ----
