contract C {
	function f(uint256 n) public pure returns (uint256) {
		uint256[][] memory a = new uint256[][](2);
		for (uint256 i = 0; i < 2; ++i)
			a[i] = new uint256[](3);
		return a[0][0] = n;
	}
}
// ====
// compileViaYul: also
// ----
// f(uint256): 42 -> 42
