pragma experimental ABIEncoderV2;
enum E { A, B, C }
contract test {
    mapping(E => uint8) table;
    function get(E k) public returns (uint8 v) {
        return table[k];
    }
    function set(E k, uint8 v) public {
        table[k] = v;
    }
}
// ====
// compileViaYul: also
// EVMVersion: >=byzantium
// ----
// get(uint8): 0 -> 0
// get(uint8): 0x01 -> 0
// get(uint8): 0x02 -> 0
// get(uint8): 0x03 -> FAILURE, 255
// get(uint8): 0x00a7 -> FAILURE, 255
// set(uint8,uint8): 0x01, 0x00a1 ->
// get(uint8): 0 -> 0
// get(uint8): 0x01 -> 0x00a1
// get(uint8): 0x00a7 -> FAILURE, 255
// set(uint8,uint8): 0x00, 0x00ef ->
// get(uint8): 0 -> 0x00ef
// get(uint8): 0x01 -> 0x00a1
// get(uint8): 0x00a7 -> FAILURE, 255
// set(uint8,uint8): 0x01, 0x05 ->
// get(uint8): 0 -> 0x00ef
// get(uint8): 0x01 -> 0x05
// get(uint8): 0x00a7 -> FAILURE, 255
