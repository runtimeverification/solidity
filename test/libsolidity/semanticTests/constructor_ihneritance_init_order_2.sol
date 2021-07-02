contract A {
    uint256 x = 42;
    function f() public returns(uint256) {
        return x;
    }
}
contract B is A {
    uint256 public y = f();
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// constructor() ->
// y() -> 42
