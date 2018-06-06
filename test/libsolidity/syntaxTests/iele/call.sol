contract test {
	constructor() public {
		this.call();
	}
}
// ----
// Warning: (42-51): Using contract member "call" inherited from the address type is deprecated. Convert the contract to "address" type to access the member, for example use "address(contract).call" instead.
// TypeError: (42-53): Low-level calls are not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
