==== Source: A ====
contract C {
	int256 private x;
	constructor (int256 p) public { x = p; }
	function getX() public returns (int256) { return x; }
}
==== Source: B ====
import "A" as M;

contract D is M.C {
	constructor (int256 p) M.C(p) public {}
}

contract A {
	function g(int256 p) public returns (int256) {
		D d = new D(p);
		return d.getX();
	}
}

// ====
// compileViaYul: also
// ----
// g(int256): -1 -> -1
// g(int256): 10 -> 10
