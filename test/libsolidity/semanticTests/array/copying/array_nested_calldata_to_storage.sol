pragma abicoder               v2;

contract c {
    uint[][] a1;
    uint[][2] a2;
    uint[2][] a3;
    uint[2][2] a4;

    function test1(uint[][] calldata c) external returns (uint, uint) {
        a1 = c;
        assert(a1[0][0] == c[0][0]);
        assert(a1[0][1] == c[0][1]);
        return (a1.length, a1[0][0] + a1[1][1]);
    }

    function test2(uint[][2] calldata c) external returns (uint, uint) {
        a2 = c;
        assert(a2[0][0] == c[0][0]);
        assert(a2[0][1] == c[0][1]);
        return (a2[0].length, a2[0][0] + a2[1][1]);
    }

    function test3(uint[2][] calldata c) external returns (uint, uint) {
        a3 = c;
        assert(a3[0][0] == c[0][0]);
        assert(a3[0][1] == c[0][1]);
        return (a3.length, a3[0][0] + a3[1][1]);
    }

    function test4(uint[2][2] calldata c) external returns (uint) {
        a4 = c;
        assert(a4[0][0] == c[0][0]);
        assert(a4[0][1] == c[0][1]);
        return (a4[0][0] + a4[1][1]);
    }
}
// ====
// compileViaYul: true
// ----
// test1(uint[][]): refargs { 0x01, 0x02, 0x01, 0x02, 23, 42, 0x01, 0x02, 23, 42 } -> 2, 65
// test2(uint[][2]): refargs { 0x01, 0x02, 23, 42, 0x01, 0x02, 23, 42 } -> 2, 65
// test3(uint[2][]): refargs { 0x01, 0x02, 23, 42, 23, 42 } -> 2, 65
// test4(uint[2][2]): refargs { 23, 42, 23, 42 } -> 65
