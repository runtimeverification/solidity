pragma abicoder               v2;

contract C {
	struct S { uint[] a; }
	function f(S calldata s) external pure returns (uint a, uint b, uint c) {
	    return (s.a.length, s.a[0], s.a[1]);
	}
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f((uint[])): refargs { 0x01, 0x02, 42, 23 } -> 2, 42, 23
