pragma experimental SMTChecker;

contract C
{
	enum D { Left, Right }
	function f(D _a) public pure {
		uint256 x = uint256(_a);
		assert(x < 10);
	}
}
// ----
