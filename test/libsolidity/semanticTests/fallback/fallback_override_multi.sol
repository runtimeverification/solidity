contract A {
    fallback (bytes calldata _input) virtual external returns (bytes memory) {
        return _input;
    }
}
contract B {
    fallback (bytes calldata _input) virtual external returns (bytes memory) {
        return "xyz";
    }
}
contract C is B, A {
    fallback () external override (B, A) {}
}
// ====
// EVMVersion: >=byzantium
// compileViaYul: also
// ----
// () : "abc" -> FAILURE, 2
// () ->
