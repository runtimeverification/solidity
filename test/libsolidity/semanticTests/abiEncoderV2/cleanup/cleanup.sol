pragma abicoder               v2;

contract C {
    function f(uint16 a, int16 b, address c, bytes3 d, bool e)
            public pure returns (uint v, uint w, uint x, uint y, uint z) {
        v = a; w = uint256(uint16(b)); x = uint256(uint160(c)); y = uint256(uint24(d)); z = e ? 1 : 0;
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f(uint16,int16,address,bytes3,bool): 1, 2, 3, 0x610000, true -> 1, 2, 3, 0x610000, true
// f(uint16,int16,address,bytes3,bool): 0x00ffffff, 0x1ffff, 0x00ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff, 0x61626364, 1 -> FAILURE, 255
// f(uint16,int16,address,bytes3,bool): 0x00ffffff, 0, 0, 0x626364, 1 -> FAILURE, 255
// f(uint16,int16,address,bytes3,bool): 0, 0x1ffff, 0, 0x6162, 1 -> FAILURE, 255
// f(uint16,int16,address,bytes3,bool): 0, 0, 0x00ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff, 0x6164, 1 -> FAILURE, 255
// f(uint16,int16,address,bytes3,bool): 0, 0, 0, 0x61626364, 1 -> FAILURE, 255
// f(uint16,int16,address,bytes3,bool): 0, 0, 0, 0x616263, 2 -> FAILURE, 255
