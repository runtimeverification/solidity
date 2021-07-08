// this test just checks that the copy loop does not mess up the stack
contract C {
    function save(bytes memory data) public returns (uint r) {
        r = 23;
        savedData = data;
        r = 24;
    }

    bytes savedData;
}
// ====
// compileViaYul: also
// ----
// save(bytes): 0 -> 24 # empty copy loop #
// save(bytes): "abcdefg" -> 24
