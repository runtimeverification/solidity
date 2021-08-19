contract C {
	function test(uint len, uint idx) public returns (uint)
	{
		uint[] memory array = new uint[](len);
		uint result = receiver(array, idx);

		for (uint i = 0; i < array.length; i++)
			require(array[i] == i + 1);

		return result;
	}
	function receiver(uint[] memory array, uint idx) public returns (uint)
	{
		for (uint i = 0; i < array.length; i++)
			array[i] = i + 1;

		return array[idx];
	}
}
// ====
// compileViaYul: also
// ----
// test(uint,uint): 0, 0 -> FAILURE, 255
// test(uint,uint): 1, 0 -> 1
// test(uint,uint): 10, 5 -> 6
// test(uint,uint): 10, 50 -> FAILURE, 255
