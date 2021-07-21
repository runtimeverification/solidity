contract C {
    function f(uint a, uint b, uint c, uint d, uint e) public returns (uint) {
        return a + b + c + d + e;
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// allowNonExistingFunctions: true
// ----
// f(uint,uint,uint,uint,uint): 1, 1, 1, 1, 1
// -> 5
// g()
// # g() does not exist #
// -> FAILURE, 1
