
contract c {
    function set(bytes memory b) public returns (bool) { data1 = b; return true; }
    function reset() public returns (bool) { data1 = data2; return true; }
    bytes data1;
    bytes data2;
}
// ====
// compileViaYul: also
// ----
// set(bytes): "12345" -> true
// storage: nonempty
// reset() -> true
// storage: empty
