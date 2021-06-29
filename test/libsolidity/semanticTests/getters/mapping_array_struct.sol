contract C {
    struct Y {
        uint a;
        uint b;
    }
    mapping(uint256 => Y[]) public m;
    mapping(uint256 => Y[3]) public n;
    constructor() {
        m[1].push().a = 1;
        m[1][0].b = 2;
        m[1].push().a = 3;
        m[1][1].b = 4;
        n[1][0].a = 7;
        n[1][0].b = 8;
        n[1][1].a = 9;
        n[1][1].b = 10;
    }
}
// ====
// compileViaYul: also
// ----
// m(uint256,uint): 0, 0 -> FAILURE, 255
// m(uint256,uint): 1, 0 -> 1, 2
// m(uint256,uint): 1, 1 -> 3, 4
// m(uint256,uint): 1, 2 -> FAILURE, 255
// n(uint256,uint): 0, 0 -> 0x00, 0x00
// n(uint256,uint): 1, 0 -> 7, 8
// n(uint256,uint): 1, 1 -> 9, 0x0a
// n(uint256,uint): 1, 2 -> 0x00, 0x00
