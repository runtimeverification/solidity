contract c {
    uint[] data;

    function enlarge(uint amount) public returns (uint) {
        while (data.length < amount) data.push();
        return data.length;
    }

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
// ----
// length() -> 0
// get(uint): 3 -> FAILURE, 255
// enlarge(uint): 4 -> 4
// length() -> 4
// set(uint,uint): 3, 4 -> true
// get(uint): 3 -> 4
// length() -> 4
// set(uint,uint): 4, 8 -> FAILURE, 255
// length() -> 4
