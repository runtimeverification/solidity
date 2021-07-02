pragma experimental SMTChecker;

contract C
{
	uint256 x;

	modifier m {
		require(x > 0);
		_;
	}

	function f() m public {
		assert(x > 0);
		x = x + 1;
	}
}
// ----
