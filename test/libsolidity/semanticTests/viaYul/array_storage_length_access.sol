contract C {
    uint[] storageArray;
    function set_get_length(uint len) public returns (uint) {
        while(storageArray.length < len)
            storageArray.push();
        return storageArray.length;
    }
}
// ====
// compileViaYul: also
// ----
// set_get_length(uint): 0 -> 0
// set_get_length(uint): 1 -> 1
// set_get_length(uint): 10 -> 10
// set_get_length(uint): 20 -> 20
// set_get_length(uint): 0x00FF -> 0x00FF
// set_get_length(uint): 0x0FFF -> 0x0FFF
// set_get_length(uint): 0x0FFFFF -> FAILURE, 5
