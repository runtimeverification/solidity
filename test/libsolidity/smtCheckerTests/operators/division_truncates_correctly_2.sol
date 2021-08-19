pragma experimental SMTChecker;
contract C {
	function f(int256 x, int256 y) public pure {
		x = 7;
		y = 2;
		assert(x / y == 3);
	}
}
// ----
