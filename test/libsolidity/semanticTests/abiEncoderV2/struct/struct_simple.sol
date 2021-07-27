pragma abicoder               v2;

contract C {
    struct S { uint a; uint8 b; uint8 c; bytes2 d; }
    function f(S memory s) public pure returns (uint a, uint b, uint c, uint d) {
        a = s.a;
        b = s.b;
        c = s.c;
        d = uint16(s.d);
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f((uint,uint8,uint8,bytes2)): refargs { 1, 0x02, 0x03, 0x6162 } -> 1, 2, 3, 0x6162
