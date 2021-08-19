contract test {
	function f() public { }
	function g() public { revert(); }
	function h() public { assert(false); }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f() ->
// g() -> FAILURE, 255
// h() -> FAILURE, 255
