contract C {
	function f(string memory s) public pure returns (bytes memory t) {
		t = bytes(s);
	}
}
// ====
// compileViaYul: also
// ----
// f(string): "Hello" -> "Hello"
