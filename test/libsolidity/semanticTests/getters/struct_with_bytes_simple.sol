contract C {
    struct S {
        uint a;
        bytes b;
        mapping(uint => uint) c;
        uint[] d;
    }
    uint shifter;
    S public s;
    constructor() {
        s.a = 7;
        s.b = "abc";
        s.c[0] = 9;
    }
}
// ----
// s() -> 0x07, "abc"
