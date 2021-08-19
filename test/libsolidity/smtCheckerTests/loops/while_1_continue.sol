pragma experimental SMTChecker;

contract C
{
	function f(uint256 x, bool b) public pure {
		require(x < 10);
		while (x < 10) {
			if (b) {
				x = 20;
				continue;
			}
			++x;
		}
		assert(x >= 10);
	}
}
// ====
// SMTSolvers: z3
