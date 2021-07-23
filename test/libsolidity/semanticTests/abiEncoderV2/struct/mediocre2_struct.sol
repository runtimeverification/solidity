pragma abicoder               v2;

contract C {
    struct S { C c; uint[] x; }
    function f(uint a, S[2] memory s1, uint b) public returns (uint r1, C r2, uint r3) {
        r1 = a;
        r2 = s1[0].c;
        r3 = b;
    }
}
// ====
// compileViaYul: also
// ----
// f(uint,(address,uint[])[2],uint): 7, refargs { refargs { 0x0000000000000000000000000000000000000000, 0x01, 0x02, 17, 18 }, refargs { 0x0000000000000000000000000000000000000099, 0x01, 0x04, 49, 50, 52, 53 } }, 8 -> 7, 0x0, 8
