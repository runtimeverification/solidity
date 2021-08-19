pragma experimental SMTChecker;

contract C
{
	function f(bool c) public pure {
		uint256[3] memory a = c ? [uint256(1), 2, 3] : [uint256(1), 2, 4];
		uint256[3] memory b = [uint256(1), 2, c ? 3 : 4];
		assert(a[0] == b[0]);
		assert(a[1] == b[1]);
		assert(a[2] == b[2]);
	}
}
// ----
