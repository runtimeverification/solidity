contract A {
    uint256[3] arr;
    bool public test = false;

    function getElement(uint256 i) public returns (uint256) {
        return arr[i];
    }

    function testIt() public returns (bool) {
        uint256 i = this.getElement(5);
        test = true;
        return true;
    }
}
// ====
// compileViaYul: also
// ----
// test() -> false
// testIt() -> FAILURE, 255
// test() -> false
