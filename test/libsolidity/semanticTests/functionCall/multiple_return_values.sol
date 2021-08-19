contract test {
    function run(bool x1, uint x2) public returns(uint y1, bool y2, uint y3) {
        y1 = x2; y2 = x1;
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// run(bool,uint): true, 0x00cd -> 0x00cd, true, 0
