pragma experimental SMTChecker;

contract Simple {
	function f() public pure {
		uint256 x = 10;
		uint256 y;
		while (y < x)
		{
			++y;
			x = 0;
			while (x < 10)
				++x;
			assert(x == 10);
		}
		// Removed because of Spacer nondeterminism.
		//assert(y == x);
	}
}
// ----
