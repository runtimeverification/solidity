pragma experimental SMTChecker;

contract C
{
	function f() public pure {
		uint256[3] memory array = [uint256(1), 2, 3];
		assert(array[0] == 1);
		assert(array[1] == 2);
		assert(array[2] == 3);
	}
}
// ----
