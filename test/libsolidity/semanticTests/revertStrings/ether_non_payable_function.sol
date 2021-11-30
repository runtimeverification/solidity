contract C {
	function f() public {}
}
// ====
// EVMVersion: >=byzantium
// revertStrings: debug
// ----
// f(), 1 wei -> FAILURE, 255
// () -> FAILURE, 1
