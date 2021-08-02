contract Creator {
    uint public r;
    address public ch;

    constructor(address[3] memory s, uint x) {
        r = x;
        ch = s[2];
    }
}
// ====
// compileViaYul: also
// ----
// constructor(): array 160 [ 1, 2, 3 ], 4 ->
// r() -> 4
// ch() -> 3
