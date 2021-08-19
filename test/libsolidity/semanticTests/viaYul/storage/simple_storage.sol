contract C {
    uint256 x;
    uint256 y;
    function setX(uint256 a) public returns (uint256 _x) {
        x = a;
        _x = x;
    }
    function setY(uint256 a) public returns (uint256 _y) {
        y = a;
        _y = y;
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// setX(uint256): 6 -> 6
// setY(uint256): 2 -> 2
