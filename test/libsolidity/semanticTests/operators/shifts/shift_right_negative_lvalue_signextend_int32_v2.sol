pragma abicoder               v2;


contract C {
    function f(int32 a, uint32 b) public returns (int32) {
        return a >> b;
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f(int32,uint32): 0x00ffffff99, 0x00 -> FAILURE, 255
// f(int32,uint32): 0x00ffffff99, 0x01 -> FAILURE, 255
// f(int32,uint32): 0x00ffffff99, 0x02 -> FAILURE, 255
// f(int32,uint32): 0x00ffffff99, 0x04 -> FAILURE, 255
// f(int32,uint32): 0x00ffffff99, 0x08 -> FAILURE, 255
