contract C {
    uint8[][2] public a;
    constructor() {
        a[1].push(3);
        a[1].push(4);
    }
}
// ====
// compileViaYul: also
// ----
// a(uint,uint): 0, 0 -> FAILURE, 255
// a(uint,uint): 1, 0 -> 3
// a(uint,uint): 1, 1 -> 4
// a(uint,uint): 2, 0 -> FAILURE, 255
