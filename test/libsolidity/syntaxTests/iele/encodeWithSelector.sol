contract test {
	constructor() {
		abi.encodeWithSelector(0);
	}
}
// ----
// TypeError 2038: (35-57): abi.encodeWithSelector not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
