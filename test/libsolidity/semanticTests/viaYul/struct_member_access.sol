pragma abicoder               v2;

contract C {
    struct S {
        uint a;
        uint[] b;
        uint c;
    }

    S s;
    constructor() {
        s.a = 42;
        s.b.push(1);
        s.b.push(2);
        s.b.push(3);
        s.c = 21;
    }

    function f(S memory m) public pure returns (uint, uint[] memory, uint) {
        return (m.a, m.b, m.c);
    }
    function g(S calldata c) external pure returns (uint, uint, uint, uint, uint, uint) {
        return (c.a, c.b.length, c.c, c.b[0], c.b[1], c.b[2]);
    }
    function g2(S calldata c1, S calldata c2) external pure returns (uint, uint, uint, uint, uint, uint) {
        return (c1.a, c1.c, c2.a, c2.b.length, c2.c, c2.b[0]);
    }
    function h() external view returns (uint, uint, uint, uint, uint, uint) {
        return (s.a, s.b.length, s.c, s.b[0], s.b[1], s.b[2]);
    }
}
// ====
// EVMVersion: >homestead
// compileViaYul: also
// ----
// f((uint,uint[],uint)): refargs { 42, 0x01, 0x03, 1, 2, 3, 21 } -> 42, dynarray 0 [ 1, 2, 3 ], 21
// g((uint,uint[],uint)): refargs { 42, 0x01, 0x03, 1, 2, 3, 21 } -> 42, 3, 21, 1, 2, 3
// g2((uint,uint[],uint),(uint,uint[],uint)): refargs {42, 0x01, 0x03, 1, 2, 3, 21 }, refargs { 7, 0x01, 0x01, 17, 9 }  -> 42, 21, 7, 1, 9, 17
// h() -> 42, 3, 21, 1, 2, 3
