pragma experimental SMTChecker;
contract C
{
	uint256 a;
	function g() public {
		if (a > 0)
		{
			a = a - 1;
			g();
		}
		else
			assert(a == 0);
	}
}

// ----
