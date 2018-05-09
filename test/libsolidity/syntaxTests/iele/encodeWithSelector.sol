contract test {
	constructor() public {
		abi.encodeWithSelector(0);
	}
}
// ----
// TypeError: (42-67): abi.encodeWithSelector not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
