contract Base {
	function f(uint n) public returns (uint) {
		return 2 * n;
	}
}

contract Child is Base {
	function g(uint n) public returns (uint) {
		return f(n);
	}
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// g(uint): 4 -> 8
