pragma abicoder               v2;
contract C {
    function f(uint[] calldata x) external returns (uint) {
        return x.length;
    }
    function f(uint[][] calldata x) external returns (uint l1, uint l2, uint l3) {
        l1 = x.length;
        if (l1 > 0) l2 = x[0].length;
        if (l1 > 1) l3 = x[1].length;
    }
    function f(uint[2] calldata x) external returns (uint) {
        return x.length;
    }
}
// ====
// compileViaYul: also
// ----
// f(uint[]): dynarray 0 [ ] -> 0
// f(uint[]): dynarray 0 [ 23 ] -> 1
// f(uint[]): dynarray 0 [ 23, 42 ] -> 2
// f(uint[]): dynarray 0 [ 23, 42, 17 ] -> 3
// f(uint[2]): array 0 [ 23, 42 ] -> 2
// f(uint[][]): refargs { 0x01, 0x00 } -> 0, 0, 0
// f(uint[][]): refargs { 0x01, 0x01, 0x01, 0x00 } -> 1, 0, 0
// f(uint[][]): refargs { 0x01, 0x01, 0x00 } -> 1, 0, 0
// f(uint[][]): refargs { 0x01, 0x01, 0x01, 0x01, 23 } -> 1, 1, 0
// f(uint[][]): refargs { 0x01, 0x01, 0x01, 0x02, 23, 42 } -> 1, 2, 0
// f(uint[][]): refargs { 0x01, 0x01, 0x01, 0x02, 23, 42 } -> 1, 2, 0
// f(uint[][]): refargs { 0x01, 0x01, 0x01, 0x01, 0x00ffe0 } -> 1, 1, 0
// f(uint[][]): refargs { 0x01, 0x02, 0x01, 0x02, 23, 42, 0x01, 0x02, 23, 42 } -> 2, 2, 2
// f(uint[][]): refargs { 0x01, 0x02, 0x01, 0x02, 23, 42, 0x01, 0x00 } -> 2, 2, 0
// f(uint[][]): refargs { 0x01, 0x02, 0x01, 0x00, 0x01, 0x02, 23, 42} -> 2, 0, 2
// f(uint[][]): refargs { 0x01, 0x02, 0x01, 0x02, 23, 42, 0x01, 0x01, 17 } -> 2, 2, 1
// f(uint[][]): refargs { 0x01, 0x02, 0x01, 0x02, 23, 42, 0x01, 0x02, 17, 13 } -> 2, 2, 2
