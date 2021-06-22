contract C {
	int[10] x;
	function f() public view {
		int[](x);
		int(x);
	}
}
// ----
// TypeError 9640: (55-63): Explicit type conversion not allowed from "int[10] storage ref" to "int[] storage pointer".
// TypeError 9640: (67-73): Explicit type conversion not allowed from "int[10] storage ref" to "int".
