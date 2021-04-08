contract test {
    function f(bool cond) public returns (uint) {
        uint8 x = 0xcd;
        uint16 y = 0xabab;
        return cond ? x : y;
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f(bool): true -> 0x00cd
// f(bool): false -> 0x00abab
