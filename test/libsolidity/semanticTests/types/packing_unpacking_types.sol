contract test {
    function run(bool a, uint32 b, uint64 c) public returns(uint y) {
        if (a) y = 1;
        y = y * 0x100000000 | ~b;
        y = y * 0x10000000000000000 | ~c;
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// run(bool,uint32,uint64): true, 0x0f0f0f0f, 0x00f0f0f0f0f0f0f0f0
// -> 0x01f0f0f0f00f0f0f0f0f0f0f0f
