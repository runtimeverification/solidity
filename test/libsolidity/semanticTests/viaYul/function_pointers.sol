contract C {
	function f() public {
		function() internal returns (uint) _f;
		_f();
	}
	function g() public {
		function() external returns (uint) _g;
		_g();
	}
	function h1() internal returns (function() internal returns (uint) _h) {}
	function h2() public {
		h1()();
	}
	function k1() internal returns (function() external returns (uint) _k) {}
	function k2() public {
		k1()();
	}
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f() -> FAILURE, 0x01
// g() -> FAILURE, 0x01
// h2() -> FAILURE, 0x01
// k2() -> FAILURE, 0x01
