pragma abicoder               v2;

contract C {
    function f(uint16 a, int16 b, address c, bytes3 d, bool e)
            public pure returns (uint v, uint w, uint x, uint y, uint z) {
        v = a; w = uint256(b); x = uint256(c); y = uint256(d); z = e ? 1 : 0;
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f(uint16,int16,address,bytes3,bool): 1, 2, 3, "a", true -> 1, 2, 3, "a", true
// f(uint16,int16,address,bytes3,bool): 0xffffff, 0x1ffff, 0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff, "abcd", 1 -> FAILURE
// f(uint16,int16,address,bytes3,bool): 0xffffff, 0, 0, "bcd", 1 -> FAILURE
// f(uint16,int16,address,bytes3,bool): 0, 0x1ffff, 0, "ab", 1 -> FAILURE
// f(uint16,int16,address,bytes3,bool): 0, 0, 0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff, "ad", 1 -> FAILURE
// f(uint16,int16,address,bytes3,bool): 0, 0, 0, "abcd", 1 -> FAILURE
// f(uint16,int16,address,bytes3,bool): 0, 0, 0, "abc", 2 -> FAILURE
