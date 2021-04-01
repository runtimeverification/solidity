contract C {
    address[] addressArray;
    function set_get_length(uint len) public returns (uint)
    {
        while(addressArray.length < len)
            addressArray.push();
        while(addressArray.length > len)
            addressArray.pop();
        return addressArray.length;
    }
}
// ====
// compileViaYul: also
// EVMVersion: >=petersburg
// ----
// set_get_length(uint): 0 -> 0
// set_get_length(uint): 1 -> 1
// set_get_length(uint): 10 -> 10
// set_get_length(uint): 20 -> 20
// set_get_length(uint): 0 -> 0
// set_get_length(uint): 0x00FF -> 0x00FF
// set_get_length(uint): 0x0FFF -> 0x0FFF
// set_get_length(uint): 0x00FFFF -> FAILURE, 5 # Out-of-gas #
