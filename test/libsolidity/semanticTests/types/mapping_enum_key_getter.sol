contract test {
    enum E { A, B, C }
    mapping(E => uint8) public table;
    function set(E k, uint8 v) public {
        table[k] = v;
    }
    function get(E k) public returns (uint8) {
        return this.table(k);
    }
}
// ====
// EVMVersion: >=byzantium
// ----
// table(uint8): 0 -> 0
// table(uint8): 0x01 -> 0
// table(uint8): 0x00a7 -> FAILURE, 255
// get(uint8): 0 -> 0
// get(uint8): 0x01 -> 0
// get(uint8): 0x00a7 -> FAILURE, 255
// set(uint8,uint8): 0x01, 0x00a1 ->
// table(uint8): 0 -> 0
// table(uint8): 0x01 -> 0x00a1
// table(uint8): 0x00a7 -> FAILURE, 255
// get(uint8): 0 -> 0
// get(uint8): 0x01 -> 0x00a1
// get(uint8): 0x00a7 -> FAILURE, 255
// set(uint8,uint8): 0x00, 0x00ef ->
// table(uint8): 0 -> 0x00ef
// table(uint8): 0x01 -> 0x00a1
// table(uint8): 0x00a7 -> FAILURE, 255
// get(uint8): 0 -> 0x00ef
// get(uint8): 0x01 -> 0x00a1
// get(uint8): 0x00a7 -> FAILURE, 255
// set(uint8,uint8): 0x01, 0x05 ->
// table(uint8): 0 -> 0x00ef
// table(uint8): 0x01 -> 0x05
// table(uint8): 0x00a7 -> FAILURE, 255
// get(uint8): 0 -> 0x00ef
// get(uint8): 0x01 -> 0x05
// get(uint8): 0x00a7 -> FAILURE, 255
