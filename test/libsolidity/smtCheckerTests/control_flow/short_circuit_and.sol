pragma experimental SMTChecker;

contract c {
	uint256 x;
	function f() internal returns (uint256) {
		x = x + 1;
		return x;
	}
	function g() public returns (bool) {
		x = 0;
		bool b = (f() == 0) && (f() == 0);
		assert(x == 1);
		assert(!b);
		return b;
	}
}
// ----
