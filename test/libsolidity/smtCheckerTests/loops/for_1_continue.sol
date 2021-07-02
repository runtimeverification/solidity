pragma experimental SMTChecker;

contract C
{
	function f(uint256 x, bool b) public pure {
		require(x < 10);
		for (; x < 10; ++x) {
			if (b) {
				x = 20;
				continue;
			}
		}
		assert(x > 0);
	}
}
// ====
// SMTSolvers: z3
