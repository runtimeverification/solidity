contract C {
    mapping(string => uint8[3]) public x;
    constructor() {
        x["abc"][0] = 1;
        x["abc"][2] = 3;
        x["abc"][1] = 2;
        x["def"][1] = 9;
    }
}
// ====
// compileViaYul: also
// ----
// x(string,uint): "abc", 0 -> 1
// x(string,uint): "abc", 1 -> 2
// x(string,uint): "abc", 2 -> 3
// x(string,uint): "def", 0 -> 0x00
// x(string,uint): "def", 1 -> 9
// x(string,uint): "def", 2 -> 0x00
