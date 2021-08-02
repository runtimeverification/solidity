contract test {
	function f() public {
		this.f.selector;
	}
}
// ----
// TypeError 2524: (41-56): Member "selector" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
