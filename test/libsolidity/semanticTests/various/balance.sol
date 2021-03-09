contract test {
    constructor() payable {}

    function getBalance() public returns (uint balance) {
        return address(this).balance;
    }
}

// ====
// compileViaYul: also
// ----
// constructor(), 23 wei ->
// getBalance() -> 23
