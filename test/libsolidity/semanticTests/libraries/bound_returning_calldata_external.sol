library D {
    function f(bytes calldata _x) public pure returns (bytes calldata) {
        return _x;
    }
    function g(bytes calldata _x) public pure returns (bytes memory) {
        return _x;
    }
}

contract C {
    using D for bytes;
    function f(bytes calldata _x) public pure returns (bytes1, bytes1) {
        return (_x.f()[0], _x.g()[0]);
    }
}
// ====
// compileViaYul: also
// EVMVersion: >homestead
// ----
// library: D
// f(bytes): "abcd" -> 0x61, 0x61
