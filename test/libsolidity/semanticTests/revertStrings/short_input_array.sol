pragma abicoder               v2;
contract C {
	function f(uint[] memory a) public pure returns (uint) { return 7; }
}
// ====
// EVMVersion: >=byzantium
// revertStrings: debug
// compileViaYul: also
// ----
// f(uint[]): refargs { 1 } -> 7
