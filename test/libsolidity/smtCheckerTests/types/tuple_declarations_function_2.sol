pragma experimental SMTChecker;

contract C
{
	function f(uint256 x) internal pure returns (uint256, bool, uint256) {
		bool b = true;
		uint256 y = 999;
		return (x * 2, b, y);
	}
	function g() public pure {
		(uint256 x, bool b, uint256 y) = f(7);
		assert(x == 14);
		assert(b);
		assert(y == 999);
	}
}
// ----
