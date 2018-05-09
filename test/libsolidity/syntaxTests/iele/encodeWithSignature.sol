contract test {
	function f() public {
		abi.encodeWithSignature("f()");
	}
}
// ----
// TypeError: (41-71): abi.encodeWithSignature not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
