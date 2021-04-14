contract C {
    function f(uint256 i) public returns (string memory) {
        string[4] memory x = ["This", "is", "an", "array"];
        return (x[i]);
    }
}

// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f(uint256): 0 -> "This"
// f(uint256): 1 -> "is"
// f(uint256): 2 -> "an"
// f(uint256): 3 -> "array"
