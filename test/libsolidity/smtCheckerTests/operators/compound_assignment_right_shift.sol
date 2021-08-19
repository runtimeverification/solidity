pragma experimental SMTChecker;

contract C {
	function f(int256 a, uint256 b) public view {
		a >>= tx.gasprice;
		require(a == 16 && b == 2);
		a >>= b;
		assert(a == 4); // should hold
	}
}
// ----
