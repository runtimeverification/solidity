contract BaseBase {
	function f(uint n) public virtual returns (uint) {
		return 2 * n;
	}
	function s(uint n) public returns (uint) {
		return 4 * n;
	}
}

contract Base is BaseBase {
	function f(uint n) public virtual override returns (uint) {
		return 3 * n;
	}
}

contract Child is Base {
	function g(uint n) public returns (uint) {
		return f(n);
	}

	function h(uint n) public returns (uint) {
		return s(n);
	}
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// g(uint): 4 -> 12
// h(uint): 4 -> 16
