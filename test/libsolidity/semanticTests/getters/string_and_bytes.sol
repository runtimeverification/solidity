contract C {
    string public a;
    string public b;
    bytes public c;
    constructor() {
        a = "hello world";
        b = hex"41424344";
        c = hex"ff077fff";
    }
}
// ----
// a() -> "hello world"
// b() -> "ABCD"
// c() -> "\xff\x07\x7f\xff"
