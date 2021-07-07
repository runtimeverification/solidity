pragma abicoder               v2;
contract C {
	function e(bytes memory a) public pure returns (uint) { return 7; }
}
// ====
// EVMVersion: >=byzantium
// revertStrings: debug
// compileViaYul: also
// ----
// e(bytes): "\x07" -> 7
