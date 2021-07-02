pragma experimental SMTChecker;

contract C
{
	function f() public pure {
		uint256 a = 1;
		uint256 b = 3;
		uint256 c = 7;
		a += b += c;
		assert(b ==  10 && a == 11);
		a += (b += c);
		assert(b ==  17 && a == 28);
		a += a += a;
		assert(a == 112);
	}
}
