contract C {
    modifier add(uint16 a, uint16 b) {
        unchecked { a + b; }
        _;
    }

    function f(uint16 a, uint16 b, uint16 c) public add(a, b) returns (uint16) {
        return b + c;
    }
}
// ----
// f(uint16,uint16,uint16): 0x00e000, 0x00e500, 2 -> 58626
// f(uint16,uint16,uint16): 0x001000, 0x00e500, 0x00e000 -> FAILURE, 255
