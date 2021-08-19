pragma abicoder v1;
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
// compileViaYul: false
// revertStrings: debug
// ----
// f() -> FAILURE, 0
// g(string): "" -> FAILURE, 0
// g(string): "" -> FAILURE, 0
