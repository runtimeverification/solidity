contract A {
    bytes public x;
    fallback (bytes calldata _input) external returns (bytes memory) {
        x = _input;
        return "";
    }
}
// ====
// EVMVersion: >=byzantium
// ----
// () : "abc" -> ""
// x() -> "abc"
