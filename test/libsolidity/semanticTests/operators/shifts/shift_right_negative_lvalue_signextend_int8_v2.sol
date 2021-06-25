pragma abicoder               v2;


contract C {
    function f(int8 a, uint8 b) public returns (int8) {
        return a >> b;
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f(int8,uint8): 0x0099, 0x00 -> FAILURE, 255
// f(int8,uint8): 0x0099, 0x01 -> FAILURE, 255
// f(int8,uint8): 0x0099, 0x02 -> FAILURE, 255
// f(int8,uint8): 0x0099, 0x04 -> FAILURE, 255
// f(int8,uint8): 0x0099, 0x08 -> FAILURE, 255
