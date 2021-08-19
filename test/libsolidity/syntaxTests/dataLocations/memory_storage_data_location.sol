contract C {
	int[] x;
	function f() public {
		int[] storage a = x;
		int[] memory b;
		a = b;
		a = int[](b);
	}
}
// ----
// TypeError 7407: (93-94): Type int[] memory is not implicitly convertible to expected type int[] storage pointer.
// TypeError 7407: (102-110): Type int[] memory is not implicitly convertible to expected type int[] storage pointer.
