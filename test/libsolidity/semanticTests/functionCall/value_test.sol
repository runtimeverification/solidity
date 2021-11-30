contract C {
	function f() public payable returns (uint) {
		return msg.value;
	}
}
// ====
// compileViaYul: also
// ----
// f(), 1000000000 wei -> 1000000000
// f(), 1 wei -> 1
