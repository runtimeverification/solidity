contract test {
	constructor() public {
		this.callcode();
	}
}
// ----
// Warning: (42-55): Using contract member "callcode" inherited from the address type is deprecated. Convert the contract to "address" type to access the member, for example use "address(contract).callcode" instead.
// TypeError: (42-57): Low-level calls are not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
