contract C {
    mapping(uint => mapping(uint => uint)) public x;
    constructor() {
        x[1][2] = 3;
    }
}
// ====
// compileViaYul: also
// ----
// x(uint,uint): 1, 2 -> 3
// x(uint,uint): 0, 0 -> 0
