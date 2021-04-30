contract C {
    uint[] storageArray;
    function test(uint v) public {
        storageArray.push(v);
    }
    function getLength() public view returns (uint) {
        return storageArray.length;
    }
    function fetch(uint a) public view returns (uint) {
        return storageArray[a];
    }
}
// ====
// compileViaYul: also
// ----
// getLength() -> 0
// test(uint): 42 ->
// getLength() -> 1
// fetch(uint): 0 -> 42
// fetch(uint): 1 -> FAILURE, 255
// test(uint): 23 ->
// getLength() -> 2
// fetch(uint): 0 -> 42
// fetch(uint): 1 -> 23
// fetch(uint): 2 -> FAILURE, 255
