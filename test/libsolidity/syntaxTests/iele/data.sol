contract test {
	constructor() public {
		msg.data;
	}
}
// ----
// TypeError: (42-50): msg.data is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
