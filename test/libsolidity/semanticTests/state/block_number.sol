contract C {
    constructor() {}
    function f() public returns (uint) {
        return block.number;
    }
}
// ====
// compileViaYul: also
// ----
// constructor()
// f() -> 3
// f() -> 4
