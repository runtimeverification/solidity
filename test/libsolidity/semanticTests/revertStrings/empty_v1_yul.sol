contract C {
	function f() public {
		revert("");
	}
	function g(string calldata msg) public {
		revert(msg);
	}
}
// ====
// ABIEncoderV1Only: true
// EVMVersion: >=byzantium
// compileViaYul: true
// revertStrings: debug
// ----
// f() -> FAILURE, 0
// g(string): "" -> FAILURE, 0
// g(string): "" -> FAILURE, 0
// g(string): "" -> FAILURE, 0
