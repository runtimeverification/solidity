pragma experimental SMTChecker;

contract C  {
	function f(uint256 x, uint256 y) public pure returns (uint256) {
		require(x >= y);
		return x - y;
	}
}
