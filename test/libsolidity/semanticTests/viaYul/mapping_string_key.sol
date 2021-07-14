contract C {
	mapping (string => uint) map;
	function set(string memory s) public {
		map[s];
	}
}
// ====
// compileViaYul: also
// ----
// set(string): "01234567890123456789012345678901" ->
