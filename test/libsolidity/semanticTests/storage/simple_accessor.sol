contract test {
    uint public data;
    constructor() {
        data = 8;
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// data() -> 8
