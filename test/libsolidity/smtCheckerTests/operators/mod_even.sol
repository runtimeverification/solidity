pragma experimental SMTChecker;

contract C
{
	function f(uint256 x) public pure {
		require(x < 10000);
		uint256 y = x * 2;
		assert((y % 2) == 0);
	}
}
// ----
