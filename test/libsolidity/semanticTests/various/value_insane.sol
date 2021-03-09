contract helper {
    function getBalance() public payable returns (uint myBalance) {
        return address(this).balance;
    }
}


contract test {
    helper h;

    constructor() payable {
        h = new helper();
    }

    function sendAmount(uint amount) public returns (uint bal) {
        return h.getBalance{value: amount + 3, gas: 1000}();
    }
}

// ====
// compileViaYul: also
// ----
// constructor(), 20 wei ->
// sendAmount(uint): 5 -> 8
