library D {
    function f(bytes calldata _x) internal pure returns (bytes1) {
        return _x[0];
    }
    function g(bytes memory _x) internal pure returns (bytes1) {
        return _x[0];
    }
}

contract C {
    using D for bytes;
    function f(bytes calldata _x) public pure returns (bytes1, bytes1) {
        return (_x.f(), _x.g());
    }
}
// ====
// compileViaYul: also
// ----
// f(bytes): "abcd" -> 0x61, 0x61
