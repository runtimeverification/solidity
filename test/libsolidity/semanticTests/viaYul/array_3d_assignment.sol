contract C {
	function f(uint256 n) public pure returns (uint256) {
		uint256[][][] memory a = new uint256[][][](2);
		for (uint256 i = 0; i < 2; ++i)
		{
			a[i] = new uint256[][](3);
			for (uint256 j = 0; j < 3; ++j)
				a[i][j] = new uint256[](4);
		}
		a[1][1][1] = n;
		uint256[][] memory b = a[1];
		uint256[] memory c = b[1];
		return c[1];
	}
}
// ====
// compileViaYul: also
// ----
// f(uint256): 42 -> 42
