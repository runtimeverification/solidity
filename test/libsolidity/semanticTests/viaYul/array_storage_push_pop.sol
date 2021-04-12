contract C {
    uint[] storageArray;
    function set_get_length(uint len) public returns (uint) {
        while(storageArray.length < len)
            storageArray.push();
        while(storageArray.length > 0)
            storageArray.pop();
        return storageArray.length;
    }
}
// ====
// compileViaYul: also
// ----
// set_get_length(uint): 0 -> 0
// set_get_length(uint): 1 -> 0
// set_get_length(uint): 10 -> 0
// set_get_length(uint): 20 -> 0
// set_get_length(uint): 0x00FF -> 0
// set_get_length(uint): 0x0FFF -> 0
// set_get_length(uint): 0x00FFFF -> FAILURE, 5 # Out-of-gas #
