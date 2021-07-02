pragma experimental SMTChecker;
contract C {
	function f(int256 x, int256 y) public pure returns (int256) {
		require(y != 0);
		require(y != -1);
		return x / y;
	}
}
// ----
