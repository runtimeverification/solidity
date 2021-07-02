pragma experimental SMTChecker;

contract c {
	uint256 x;
	function f() internal returns (uint256) {
		x = x + 1;
		return x;
	}
	function g() public returns (bool) {
		x = 0;
		bool b = (f() > 1) || (f() > 1);
		assert(x == 2);
		assert(b);
		return b;
	}
}
// ----
