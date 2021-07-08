pragma abicoder               v2;
contract C {
    function f(uint256[][] calldata x) external { x[0]; }
}
// ====
// EVMVersion: >=byzantium
// revertStrings: debug
// compileViaYul: also
// ----
// f(uint256[][]): refargs { 0x01, 0x01, 0x01, 0x02, 0x42, 0x43 } -> 
