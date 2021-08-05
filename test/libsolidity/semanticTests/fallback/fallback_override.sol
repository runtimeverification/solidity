contract A {
    fallback (bytes calldata _input) virtual external returns (bytes memory) {
        return _input;
    }
}
contract B is A {
    fallback (bytes calldata _input) override external returns (bytes memory) {
        return "xyz";
    }
}
// ====
// EVMVersion: >=byzantium
// compileViaYul: also
// ----
// () : "abc" -> "xyz"
