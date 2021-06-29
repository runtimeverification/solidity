contract test {
    uint value1;
    uint value2;
    function get(uint8 which) public returns (uint value) {
        if (which == 0) return value1;
        else return value2;
    }
    function set(uint8 which, uint value) public {
        if (which == 0) value1 = value;
        else value2 = value;
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// get(uint8): 0x00 -> 0
// get(uint8): 0x01 -> 0
// set(uint8,uint): 0x00, 0x1234 ->
// set(uint8,uint): 0x01, 0x008765 ->
// get(uint8): 0x00 -> 0x1234
// get(uint8): 0x01 -> 0x008765
// set(uint8,uint): 0x00, 0x03 ->
// get(uint8): 0x00 -> 0x03
