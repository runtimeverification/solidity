contract A {
    uint public x;
    fallback (bytes calldata _input) external returns (bytes memory) {
        x = _input.length;
        return "";
    }
}
// ====
// compileViaYul: also
// EVMVersion: >=byzantium
// ----
// () : "abc" -> ""
// x() -> 3
