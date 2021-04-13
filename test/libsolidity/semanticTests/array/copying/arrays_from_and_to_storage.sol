contract Test {
    uint24[] public data;
    function set(uint24[] memory _data) public returns (uint) {
        data = _data;
        return data.length;
    }
    function get() public returns (uint24[] memory) {
        return data;
    }
}
// ====
// compileViaYul: also
// ----
// set(uint24[]): dynarray 24 [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18] -> 18
// data(uint): 7 -> 8
// data(uint): 15 -> 16
// data(uint): 18 -> FAILURE, 255
// get() -> dynarray 24 [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18]
