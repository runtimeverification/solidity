contract c {
    uint[4] data;

    function set(uint index, uint value) public returns (bool) {
        data[index] = value;
        return true;
    }

    function get(uint index) public returns (uint) {
        return data[index];
    }

    function length() public returns (uint) {
        return data.length;
    }
}

// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// length() -> 4
// set(uint,uint): 3, 4 -> true
// set(uint,uint): 4, 5 -> FAILURE, hex"4e487b71", 0x32
// set(uint,uint): 400, 5 -> FAILURE, hex"4e487b71", 0x32
// get(uint): 3 -> 4
// get(uint): 4 -> FAILURE, hex"4e487b71", 0x32
// get(uint): 400 -> FAILURE, hex"4e487b71", 0x32
// length() -> 4
