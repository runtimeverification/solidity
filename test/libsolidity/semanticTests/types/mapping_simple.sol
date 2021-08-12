contract test {
    mapping(uint8 => uint8) table;
    function get(uint8 k) public returns (uint8 v) {
        return table[k];
    }
    function set(uint8 k, uint8 v) public {
        table[k] = v;
    }
}
// ====
// compileViaYul: also
// ----
// get(uint8): 0 -> 0
// get(uint8): 0x01 -> 0
// get(uint8): 0x00a7 -> 0
// set(uint8,uint8): 0x01, 0x00a1 ->
// get(uint8): 0 -> 0
// get(uint8): 0x01 -> 0x00a1
// get(uint8): 0x00a7 -> 0
// set(uint8,uint8): 0x00, 0x00ef ->
// get(uint8): 0 -> 0x00ef
// get(uint8): 0x01 -> 0x00a1
// get(uint8): 0x00a7 -> 0
// set(uint8,uint8): 0x01, 0x05 ->
// get(uint8): 0 -> 0x00ef
// get(uint8): 0x01 -> 0x05
// get(uint8): 0x00a7 -> 0
