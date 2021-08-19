contract C {
    function f() external payable {}
	function g(address a) external pure {
		a.call{value: 42};
		a.call{gas: 42};
		a.staticcall{gas: 42};
		a.delegatecall{gas: 42};
	}
	function h() external view {
		this.f{value: 42};
		this.f{gas: 42};
	}
}
// ----
// TypeError 6198: (91-97): Low-level calls are not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 6198: (112-118): Low-level calls are not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 6198: (131-143): Low-level calls are not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 6198: (156-170): Low-level calls are not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
