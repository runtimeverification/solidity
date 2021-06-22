contract C {
	function basic() public pure returns(bool) {
		uint256 uint_min = type(uint256).min;
		require(uint_min == 0);

		uint256 uint_max = type(uint256).max;
		require(uint_max == 2**256 - 1);
		require(uint_max == 115792089237316195423570985008687907853269984665640564039457584007913129639935);

		int256 int_min = type(int256).min;
		require(int_min == -2**255);
		require(int_min == -57896044618658097711785492504343953926634992332820282019728792003956564819968);

		int256 int_max = type(int256).max;
		require(int_max == 2**255 -1);
		require(int_max == 57896044618658097711785492504343953926634992332820282019728792003956564819967);

		return true;
	}
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// basic() -> true
