contract A {
    fallback (bytes calldata _input) external returns (bytes memory) {
        return _input;
    }
}
// ====
// compileViaYul: also
// EVMVersion: >=byzantium
// ----
// () : "abc" -> "abc"
