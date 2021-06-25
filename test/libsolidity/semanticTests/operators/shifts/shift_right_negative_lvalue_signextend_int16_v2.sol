pragma abicoder               v2;


contract C {
    function f(int16 a, uint16 b) public returns (int16) {
        return a >> b;
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f(int16,uint16): 0x00ff99, 0x00 -> FAILURE, 255
// f(int16,uint16): 0x00ff99, 0x01 -> FAILURE, 255
// f(int16,uint16): 0x00ff99, 0x02 -> FAILURE, 255
// f(int16,uint16): 0x00ff99, 0x04 -> FAILURE, 255
// f(int16,uint16): 0x00ff99, 0x08 -> FAILURE, 255
