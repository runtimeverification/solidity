pragma experimental SMTChecker;
contract C
{
	// Used to crash because Literal had no type
	int256[3] d;
	// Used to crash because Literal had no type
	int256[3*1] x;
}
// ----
