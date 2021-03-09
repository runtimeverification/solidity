contract c {
    function save(bytes b) external { data = b; }
    function del() public returns (bool) { delete data; return true; }
    bytes data;
}
// ====
// compileViaYul: also
// ----
// save(): "1234" ->
// storage: nonempty
// del() -> true
// storage: empty
