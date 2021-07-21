contract c {
    function set(bytes memory b) public returns (bool) {
        data = b;
        return true;
    }

    function getLength() public returns (uint) {
        return data.length;
    }

    bytes data;
}
// ====
// compileViaYul: also
// ----
// getLength() -> 0
// set(bytes): "12" -> true
// getLength() -> 2
