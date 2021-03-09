contract A {
    constructor() {
        address(this).send(0);
    }
    function() payable { throw; }
}


contract B {
    uint256 public test = 1;

    function testIt() public {
        A a = new A();
        ++test;
    }
}

// ====
// compileViaYul: also
// ----
// testIt() ->
// test() -> 2
