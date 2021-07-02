contract C {
	function f(uint256 n) public pure returns (uint256) {
		uint256[][] memory a = new uint256[][](2);
		for (uint256 i = 0; i < 2; ++i)
			a[i] = new uint256[](3);
		a[1][1] = n;
		uint256[] memory b = a[1];
		return b[1];
	}
}
// ====
// compileViaYul: also
// ----
// f(uint256): 42 -> 42
