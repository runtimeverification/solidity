pragma experimental SMTChecker;

contract C {
	function f(bool b) public pure {
		uint256 v = 1000000;
		if (b)
			v <<= 2;
		assert(v > 0);
	}
}
// ----
