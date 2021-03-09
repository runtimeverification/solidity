contract test {
    function f(uint k) public returns (uint) {
        return k;
    }
}

// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f(uint): 9 -> 9
