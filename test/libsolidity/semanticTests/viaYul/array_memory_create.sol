contract C {
	function create(uint len) public returns (uint)
	{
		uint[] memory array = new uint[](len);
		return array.length;
	}
}
// ====
// compileViaYul: also
// ----
// create(uint): 0 -> 0
// create(uint): 7 -> 7
// create(uint): 10 -> 10
