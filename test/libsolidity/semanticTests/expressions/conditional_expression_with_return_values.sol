contract test {
    function f(bool cond, uint v) public returns (uint a, uint b) {
        cond ? a = v : b = v;
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f(bool,uint): true, 20 -> 20, 0
// f(bool,uint): false, 20 -> 0, 20
