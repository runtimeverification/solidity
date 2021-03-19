contract C {
    uint256 public a;
    uint256[3] public b;

    constructor(uint256 _a, uint256[3] memory _b) {
        a = _a;
        b = _b;
    }
}

// ====
// compileViaYul: also
// ----
// constructor(): 1, 2, 3, 4 ->
// a() -> 1
// b(uint): 0 -> 2
// b(uint): 1 -> 3
// b(uint): 2 -> 4
