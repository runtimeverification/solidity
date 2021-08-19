pragma abicoder               v2;
contract C {
	function f() public {
		revert("");
	}
	function g(string calldata msg) public {
		revert(msg);
	}
}
// ====
// EVMVersion: >=byzantium
// compileViaYul: also
// revertStrings: debug
// ----
// f() -> 
// g(string): "" -> 
// g(string): "" -> 
// g(string): "" -> 
