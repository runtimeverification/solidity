pragma experimental SMTChecker;
contract C {
	function f(bool b) public pure {
		uint256 x;
		if (b) ++(x);
		if (b) --(x);
		if (b) delete(b);
		assert(x == 0);
		assert(!b);
	}
}
