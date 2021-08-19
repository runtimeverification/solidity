contract C {
	function f() public pure returns (uint, uint, bytes32) {
		uint a;
		bytes32 b;
		(a,) = f();
		(,b) = f();
	}
}
// ----
// TypeError 7407: (103-106): Type tuple(uint,uint,bytes32) is not implicitly convertible to expected type tuple(uint,).
// TypeError 7407: (117-120): Type tuple(uint,uint,bytes32) is not implicitly convertible to expected type tuple(,bytes32).
