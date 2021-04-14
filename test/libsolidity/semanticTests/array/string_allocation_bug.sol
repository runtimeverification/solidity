contract Sample {
    struct s {
        uint16 x;
        uint16 y;
        string a;
        string b;
    }
    s[2] public p;

    constructor() {
        s memory m;
        m.x = 0xbbbb;
        m.y = 0xcccc;
        m.a = "hello";
        m.b = "world";
        p[0] = m;
    }
}
// ----
// p(uint): 0x0 -> 0x00bbbb, 0x00cccc, "hello", "world"
