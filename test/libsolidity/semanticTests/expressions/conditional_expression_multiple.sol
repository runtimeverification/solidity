contract test {
    function f(uint x) public returns(uint d) {
        return x > 100 ?
                    x > 1000 ? 1000 : 100
                    :
                    x > 50 ? 50 : 10;
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f(uint): 1001 -> 1000
// f(uint): 500 -> 100
// f(uint): 80 -> 50
// f(uint): 40 -> 10
