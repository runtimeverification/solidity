pragma abicoder               v2;
contract C {
	function f(uint256[][] calldata a) external returns (uint) {
		return 42;
	}
}
// ====
// EVMVersion: >=byzantium
// revertStrings: debug
// compileViaYul: also
// ----
// f(uint256[][]): refargs { 1 } -> 42
