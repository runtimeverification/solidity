pragma experimental SMTChecker;

library L
{
	function add(uint256 x, uint256 y) internal pure returns (uint256) {
		require(x < 1000);
		require(y < 1000);
		return x + y;
	}
}

contract C
{
	using L for uint256;
	function f(uint256 x) public pure {
		uint256 y = x.add(999);
		assert(y < 10000);
	}
}
