contract C {
    function t(uint) public pure {}
}
// ====
// EVMVersion: >=byzantium
// compileViaYul: true
// revertStrings: debug
// ----
// t(uint) -> FAILURE, 2
