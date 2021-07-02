contract A {
    uint256 x;
    constructor() {
        x = 42;
    }
    function f() public returns(uint256) {
        return x;
    }
}
contract B is A {
    uint256 public y = f();
}
// ====
// compileViaYul: true
// compileToEwasm: also
// ----
// constructor() ->
// y() -> 42
