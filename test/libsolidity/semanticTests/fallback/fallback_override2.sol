contract A {
    fallback (bytes calldata _input) virtual external returns (bytes memory) {
        return _input;
    }
}
contract B is A {
    fallback () override external {
    }
}
// ====
// EVMVersion: >=byzantium
// compileViaYul: also
// ----
// () : "abc" -> FAILURE, 2 
// ()  ->  
