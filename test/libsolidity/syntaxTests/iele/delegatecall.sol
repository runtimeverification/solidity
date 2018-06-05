contract test {
	constructor() public {
		this.delegatecall();
	}
}
// ----
// Warning: (42-59): Using contract member "delegatecall" inherited from the address type is deprecated. Convert the contract to "address" type to access the member, for example use "address(contract).delegatecall" instead.
// TypeError: (42-61): Low-level calls are not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
