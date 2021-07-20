pragma abicoder               v2;

contract C {
	struct S2 { uint b; }
	struct S { uint a; S2[] children; }
	function f(S calldata s) external pure returns (uint, uint, uint, uint) {
		return (s.children.length, s.a, s.children[0].b, s.children[1].b);
	}
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f((uint,(uint)[])): refargs { 17, 0x01, 0x02, 23, 42 } -> 2, 17, 23, 42
