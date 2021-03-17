contract c {
    function set(uint key, bytes b) public returns (bool) { data[key] = b; return true; }
    function copy(uint from, uint to) public returns (bool) { data[to] = data[from]; return true; }
    mapping(uint => bytes) data;
}
// ====
// compileViaYul: also
// ----
// set(uint): 1, "2" -> true
// set(uint): 2, "2345" -> true
// storage: nonempty
// copy(uint,uint): 1, 2 -> true
// storage: nonempty
// copy(uint,uint): 99, 1 -> true
// storage: nonempty
// copy(uint,uint): 99, 2 -> true
// storage: empty
