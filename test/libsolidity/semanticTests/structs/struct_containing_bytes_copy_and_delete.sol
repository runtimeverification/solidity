contract c {
    struct Struct { uint a; bytes data; uint b; }
    Struct data1;
    Struct data2;
    function set(uint _a, bytes calldata _data, uint _b) external returns (bool) {
        data1.a = _a;
        data1.b = _b;
        data1.data = _data;
        return true;
    }
    function copy() public returns (bool) {
        data1 = data2;
        return true;
    }
    function del() public returns (bool) {
        delete data1;
        return true;
    }
    function test(uint i) public returns (bytes1) {
        return data1.data[i];
    }
}
// ====
// compileViaYul: also
// ----
// storage: empty
// set(uint,bytes,uint): 12, 0x60, 13, 33, "12345678901234567890123456789012", "3" -> true
// test(uint): 32 -> "3"
// storage: nonempty
// copy() -> true
// storage: empty
// set(uint,bytes,uint): 12, 0x60, 13, 33, "12345678901234567890123456789012", "3" -> true
// storage: nonempty
// del() -> true
// storage: empty
