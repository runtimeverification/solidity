contract C {
	function f(address a) external view returns (bool success) {
		(success,) = a.call{gas: 42}("");
	}
	function h() external payable {}
	function i() external view {
		this.h{gas: 42}();
	}
}
// ----
// TypeError 6198: (90-96): Low-level calls are not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
