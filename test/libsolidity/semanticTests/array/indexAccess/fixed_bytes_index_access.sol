contract C {
    bytes16[] public data;

    function f(bytes32 x) public returns (bytes1) {
        return x[2];
    }

    function g(bytes32 x) public returns (uint256) {
        data = [x[0], x[1], x[2]];
        data[0] = "12345";
        return uint256(uint8(data[0][4]));
    }
}
// ====
// compileViaYul: also
// ----
// f(bytes32): 0x3738390000000000000000000000000000000000000000000000000000000000 -> 0x39
// g(bytes32): 0x3738390000000000000000000000000000000000000000000000000000000000 -> 0x35
// data(uint): 0x01 -> 0x38000000000000000000000000000000
