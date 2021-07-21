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
// set(uint,bytes,uint): 12, "123456789012345678901234567890123", 13 -> true
// test(uint): 32 -> 0x33
// storage: nonempty
// copy() -> true
// storage: empty
// set(uint,bytes,uint): 12, "123456789012345678901234567890123", 13 -> true
// storage: nonempty
// del() -> true
// storage: empty
