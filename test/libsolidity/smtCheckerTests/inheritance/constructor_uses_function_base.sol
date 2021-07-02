pragma experimental SMTChecker;

contract A {
	uint256 x;
	constructor() {
		x = 42;
	}
	function f() public view returns(uint256) {
		return x;
	}
}
contract B is A {
	uint256 y = f();
}
contract C is B {
	function g() public view {
		assert(y == 42);
	}
}
