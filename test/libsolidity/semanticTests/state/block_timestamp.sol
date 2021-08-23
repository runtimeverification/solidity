contract C {
    constructor() {}
    function f() public returns (uint) {
        return block.timestamp;
    }
}
// ====
// compileViaYul: also
// ----
// constructor()        # This is the 1st block #
// f() -> timestamp     # This is the 2nd block #
// f() -> timestamp     # This is the 3rd block #
