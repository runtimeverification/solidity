contract C {
    function add(uint16 a, uint16 b) public returns (uint16) {
        return a + b;
    }

    function f(uint16 a, uint16 b, uint16 c) public returns (uint16) {
        unchecked { return add(a, b) + c; }
    }
}
// ====
// compileViaYul: also
// ----
// f(uint16,uint16,uint16): 0x00e000, 0x00e500, 2 -> FAILURE, 255
// f(uint16,uint16,uint16): 0x00e000, 0x001000, 0x001000 -> 0x00
