contract C {
    function e() public pure {
        revert("Transaction failed.");
    }
    function f(bool _value) public pure {
        string memory message;
        require(_value, message);
    }
    function g(bool _value) public pure {
        require(_value, "Value is false.");
    }
    function h() public pure returns (uint) {
        assert(false);
    }
}
// ====
// compileToEwasm: also
// EVMVersion: >homestead
// allowNonExistingFunctions: true
// compileViaYul: also
// ----
// _() -> FAILURE, 1
// e() -> FAILURE, "Transaction failed."
// f(bool): false -> FAILURE, hex"08c379a0", 0x20, 0x00
// g(bool): false -> FAILURE, "Value is false."
// h() -> FAILURE, 255
