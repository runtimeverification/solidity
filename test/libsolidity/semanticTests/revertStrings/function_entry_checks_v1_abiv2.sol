pragma abicoder v2;
contract C {
    function t(uint) public pure {}
}
// ====
// EVMVersion: >=byzantium
// compileViaYul: false
// revertStrings: debug
// ----
// t(uint) -> FAILURE, 2
