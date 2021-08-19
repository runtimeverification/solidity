pragma experimental SMTChecker;

contract C {
	uint256 x;
	function f() public {
		x = 0;
		((inc))();
		assert(x == 1); // should hold
	}

	function inc() internal returns (uint256) {
		require(x < 100);
		return ++x;
	}
}
