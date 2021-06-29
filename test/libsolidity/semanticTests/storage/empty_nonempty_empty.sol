contract Test {
    bytes x;
    function set(bytes memory _a) public { x = _a; }
}
// ====
// compileViaYul: also
// ----
// set(bytes): "abc"
// storage: nonempty
// set(bytes): ""
// storage: empty
// set(bytes): "1234567890123456789012345678901"
// storage: nonempty
// set(bytes): "12345678901234567890123456789012XXXX"
// storage: nonempty
// set(bytes): "abc"
// storage: nonempty
// set(bytes): ""
// storage: empty
// set(bytes): "abc"
// storage: nonempty
// set(bytes): "12345678901234567890123456789012XXXX"
// storage: nonempty
// set(bytes): ""
// storage: empty
// set(bytes): "123456789012345678901234567890121234567890123456789012345678901212"
// storage: nonempty
// set(bytes): "abc"
// storage: nonempty
// set(bytes): ""
// storage: empty
