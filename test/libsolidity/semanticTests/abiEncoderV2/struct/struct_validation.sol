pragma abicoder               v2;

contract C {
	struct S { int16 a; uint8 b; bytes2 c; }
	function f(S memory s) public pure returns (uint a, uint b, uint c) {
		a = uint256(s.a);
		b = s.b;
		c = uint256(s.c);
	}
}
// ====
// compileViaYul: also
// ----
// f((int16,uint8,bytes2)): 0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff01, 0xff, "ab" -> 0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff01, 0xff, "ab"
// f((int16,uint8,bytes2)): 0xff010, 0xff, "ab" -> FAILURE
// f((int16,uint8,bytes2)): 0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff01, 0xff0002, "ab" -> FAILURE
// f((int16,uint8,bytes2)): 0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff01, 0xff, "abcd" -> FAILURE
